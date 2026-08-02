// =================================================================
// src/gui/MainWindow.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
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

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

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

warren@wpratt.com

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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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

#include "MainWindow.h"
#include "ConnectionPanel.h"
#include "NetworkDiagnosticsDialog.h"
#include "SupportDialog.h"
#include "AboutDialog.h"
#include "SpectrumWidget.h"
// Phase 3F Sub-Epic D Task 10: +PAN bottom-bar dropdown reads slice
// state + drives PanadapterStack layout/float actions.
#include "PanadapterStack.h"
#include "PanadapterApplet.h"
#include "PanLayoutDialog.h"
#include "core/FFTRouter.h"
#include "StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "widgets/VfoWidget.h"
#include "widgets/RxDashboard.h"
#include "widgets/AntennaSwitchToast.h"
#include "widgets/StatusToast.h"
#include "widgets/FilterPolicyDialog.h"
#include "widgets/TxBoundConfirmDialog.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"  // H.2: setTxChannel wiring
#include "core/ReceiverManager.h"
#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/PaTempUnit.h"
#include "core/RadioStatus.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/FFTEngine.h"
#include "core/NbFamily.h"
#include "core/ClarityController.h"
#include "core/StepAttenuatorController.h"
#include "core/MoxController.h"  // 3M-1a G.1: F.2 connect (hardwareFlipped → onMoxHardwareFlipped)
#include "core/NoiseFloorTracker.h"
#include "core/BoardCapabilities.h"
#include "core/TxSliceArbiter.h"  // Phase 3F Sub-Epic C Task 9: TX-handoff routing
#include "models/PanadapterModel.h"
#include "models/Band.h"
#include "models/TransmitModel.h"
#include "core/LogCategories.h"
#include "containers/ContainerManager.h"
#include "containers/ContainerWidget.h"
#include "containers/ContainerSettingsDialog.h"
#include "meters/MeterWidget.h"
#include "meters/MeterItem.h"
#include "meters/ItemGroup.h"
#include "meters/MeterPoller.h"
#include "meters/VfoDisplayItem.h"  // 3M-1c L.3 — TX badge routing
#include "applets/AppletPanelWidget.h"
#include "SMeterWidget.h"            // Task 41 (Phase 3P-II): SMeterWidget header wiring
#include "applets/AmpApplet.h"
#include "applets/Rf2ksApplet.h"
#include "applets/AppletVisibilityController.h"
#include "applets/RxApplet.h"
#include "core/PgxlConnection.h"
#include "core/TgxlConnection.h"
#include "core/Rf2ksConnection.h"
#include "core/SmartSdrApiListener.h"
#include "applets/TxApplet.h"
#include "applets/TxEqDialog.h"
// Phase 3J-2 H1: Tools menu modeless singletons (Spot Hub + FreeDV Reporter).
#include "SpotHubDialog.h"
#include "FreeDVReporterDialog.h"
// Phase 3F Sub-Epic G T4: bench-minimum Diversity dialog (Tools menu).
#include "DiversityDialog.h"
#include "models/SpotModel.h"
#include "models/FreeDVStationModel.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "core/DxClusterClient.h"
#include "core/WsjtxClient.h"
#include "core/SpotCollectorClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "PsForm.h"
#include "PsaIndicatorWidget.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "applets/PhoneCwApplet.h"
#include "applets/RadeApplet.h"
#include "applets/EqApplet.h"
#include "applets/VaxApplet.h"
#include "applets/DigitalApplet.h"
#include "applets/PureSignalApplet.h"
#include "applets/DiversityApplet.h"
#include "applets/CwxApplet.h"
#include "applets/DvkApplet.h"
#include "applets/CatApplet.h"
#include "applets/TunerApplet.h"
// Phase 23: TCI server + applets (guarded so non-WebSocket builds still compile)
#ifdef HAVE_WEBSOCKETS
#  include "applets/TciApplet.h"
#  include "applets/ClientChainApplet.h"
#  include "core/TciServer.h"
#  include "setup/TciLogWindow.h"  // Phase 3J-1 closeout Item 2 (2026-05-12)
#  include <QWebSocket>
#endif
#include "SpectrumOverlayPanel.h"
#include "SetupDialog.h"
#include "setup/DspSetupPages.h"   // NrAnfSetupPage::selectSubtab
#include "TitleBar.h"
#include "VaxFirstRunDialog.h"
#if defined(Q_OS_LINUX)
#  include "VaxLinuxFirstRunDialog.h"
#endif
#include "widgets/MasterOutputWidget.h"
#include "widgets/StationBlock.h"
#include "widgets/MetricLabel.h"
#include "widgets/StatusBadge.h"
#include "widgets/AdcOverloadBadge.h"
#include "widgets/OverflowChip.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/VirtualCableDetector.h"
#include "core/audio/RealtimeAudioPriority.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSlider>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QMenuBar>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QAction>
#include <QSignalBlocker>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QProgressDialog>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QFile>          // /proc/stat reader for Linux system-CPU path
#include <QPushButton>
#include <QCursor>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QPointer>
#include <QShortcut>

#include <cstdlib>

// Cross-platform CPU usage readers — see readProcessCpuPercent and
// readSystemCpuPercent below. POSIX side (macOS / Linux) shares
// getrusage for process CPU; macOS adds host_processor_info for system
// CPU; Linux reads /proc/stat; Windows uses GetProcessTimes /
// GetSystemTimes. Each branch is gated by Q_OS_*.
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <sys/resource.h>
#endif
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NereusSDR {

namespace {
// First-run/rescan wants the "relevant" virtual cables for the current
// platform — 3rd-party cables on Windows (BYO), our own NereusSdrVax
// entries on Mac/Linux (native HAL plugin / pipe-source). Centralising
// the platform split here keeps checkVaxFirstRun() focused on
// scenario-selection + dialog wiring.
QVector<DetectedCable> detectedForFirstRun()
{
#if defined(Q_OS_WIN)
    return VirtualCableDetector::scanThirdPartyOnly();
#else
    QVector<DetectedCable> out;
    for (const auto& c : VirtualCableDetector::scan()) {
        if (c.product == VirtualCableProduct::NereusSdrVax) {
            out.push_back(c);
        }
    }
    return out;
#endif
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_radioModel(new RadioModel(this))
{
    // ── Phase 23 (bench fix 2026-05-10): TCI Server BEFORE buildUI ───────────
    // TciApplet + ClientChainApplet are constructed by populateDefaultMeter()
    // (called from buildUI), gated on `if (m_tciServer)`. The original Phase
    // 23 wiring constructed TciServer AFTER buildUI, so the gate evaluated
    // false and the applets never appeared in the right-side panel.
    // RadioModel is already constructed via the member initializer list, so
    // it's safe to instantiate TciServer here. Auto-start is deferred to
    // after buildUI so the indicator + applets are ready to receive signals.
#ifdef HAVE_WEBSOCKETS
    {
        m_tciServer = new TciServer(m_radioModel, this);
        connect(m_tciServer, &TciServer::serverStarted,
                this, [this](quint16) { m_tciServerRunning = true;  updateTciIndicator(); });
        connect(m_tciServer, &TciServer::serverStopped,
                this, [this]()        { m_tciServerRunning = false; updateTciIndicator(); });
        connect(m_tciServer, &TciServer::clientConnected,
                this, [this](QWebSocket*) { ++m_tciClientCount; updateTciIndicator(); });
        connect(m_tciServer, &TciServer::clientDisconnected,
                this, [this](QWebSocket*) {
                    if (m_tciClientCount > 0) { --m_tciClientCount; }
                    updateTciIndicator();
                });
        connect(m_tciServer, &TciServer::txAudioActiveClientChanged,
                this, [this](QWebSocket* owner) {
                    m_tciHasTxClient = (owner != nullptr);
                    updateTciIndicator();
                    // Phase 3J-1 bench fix (2026-05-10): gate the TxWorkerThread
                    // mic-source pump while TCI is providing audio so it does
                    // not race feedTxAudioFromTci's dispatch.  See
                    // TxChannel::m_tciAudioActive doc-comment for the full
                    // narrative on why the two-source race corrupts on-air
                    // audio at the I/Q ring buffer.
                    if (auto* wdsp = m_radioModel ? m_radioModel->wdspEngine() : nullptr) {
                        if (auto* txCh = wdsp->txChannel(WdspEngine::kTxChannelId)) {
                            txCh->setTciAudioActive(owner != nullptr);
                        }
                    }
                });
    }
#endif

    buildUI();
    buildMenuBar();

    // Phase 3O Sub-Phase 10 Task 10c — host the menu bar + master-output
    // controls in a custom TitleBar strip. Must run AFTER buildMenuBar()
    // so the menu is fully populated with actions before we re-parent it.
    //
    // setMenuWidget() hands ownership to QMainWindow and installs the
    // strip at the top of the window. On macOS this also disables Qt's
    // promotion of the menu bar to the native global bar — menus render
    // in-window alongside the master-output controls (explicit design
    // choice, user-approved option D for Sub-Phase 10).
    m_titleBar = new TitleBar(m_radioModel->audioEngine(), this);
    m_titleBar->setMenuBar(menuBar());
    setMenuWidget(m_titleBar);

    // Wire the MasterOutputWidget device picker → AudioEngine so picking
    // an output device rebuilds the speakers bus. Task 10b exposes only
    // the deviceName; we load the rest of the persisted speakers config
    // (sampleRate / channels / bufferSamples / exclusiveMode / etc.) so
    // the user's tuned values are preserved across a device change.  A
    // bare default-constructed AudioDeviceConfig with only deviceName
    // set would otherwise clobber persisted fields, defeating the
    // Setup → Devices page entirely.
    connect(m_titleBar->masterOutput(), &MasterOutputWidget::outputDeviceChanged,
            this, [this](const QString& name) {
        AudioDeviceConfig cfg = AudioDeviceConfig::loadFromSettings(
            QStringLiteral("audio/Speakers"));
        cfg.deviceName = name;
        if (auto* engine = m_radioModel->audioEngine()) {
            engine->setSpeakersConfig(cfg);
        }
    });

    // Phase 3O Sub-Phase 10 Task 10d — the 💡 feature-request button
    // now lives inside TitleBar (consolidated from the old featureBar
    // QToolBar). Wire its click signal to the existing slot.
    connect(m_titleBar, &TitleBar::featureRequestClicked,
            this, &MainWindow::showFeatureRequestDialog);

    // ── Phase 3Q Sub-PR-4 D.2: ConnectionSegment wiring ────────────────────
    {
        auto* seg = m_titleBar->connectionSegment();

        // 1. State dot + pulse: driven by connectionStateChanged.
        connect(m_radioModel, &RadioModel::connectionStateChanged,
                seg, &ConnectionSegment::setState);

        // 2. frameTick: forwarded from RadioModel so we never need to
        //    re-wire when m_connection is recreated.
        connect(m_radioModel, &RadioModel::frameReceived,
                seg, &ConnectionSegment::frameTick);

        // 3. 1 Hz rate refresh — polls connection().{tx,rx}ByteRate(1000).
        auto* rateTimer = new QTimer(this);
        rateTimer->setInterval(1000);
        connect(rateTimer, &QTimer::timeout, this, [this, seg]() {
            if (auto* conn = m_radioModel->connection()) {
                // setRates(rxMbps, txMbps) — first arg names the "radio→client"
                // direction (m_rxMbps), second names "client→radio" (m_txMbps).
                // Earlier revisions passed these reversed, which made the ▲/▼
                // glyphs read in radio perspective rather than the client's.
                // Spec §Affordances reads the segment from the operator's
                // (client's) point of view: ▲ = NereusSDR uploading to radio
                // (commands), ▼ = radio downloading to NereusSDR (I/Q).
                seg->setRates(conn->rxByteRate(1000), conn->txByteRate(1000));
            }
        });
        rateTimer->start();

        // 4. RTT — wire from RadioConnection::pingRttMeasured.
        //    Re-wired on every Connecting/Probing transition so the segment
        //    always follows the live connection object (RadioModel recreates
        //    RadioConnection on each connect cycle).
        auto wireRtt = [this, seg]() {
            if (auto* conn = m_radioModel->connection()) {
                connect(conn, &RadioConnection::pingRttMeasured,
                        seg, &ConnectionSegment::setRttMs,
                        Qt::UniqueConnection);
            }
        };
        wireRtt();
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [wireRtt](ConnectionState s) {
                    if (s == ConnectionState::Connecting ||
                        s == ConnectionState::Probing) {
                        wireRtt();
                    }
                });

        // 5. Audio flow-state → ♪ pip color.
        if (auto* engine = m_radioModel->audioEngine()) {
            connect(engine, &AudioEngine::flowStateChanged,
                    seg, &ConnectionSegment::setAudioFlowState);
        }

        // 6. Click affordances.
        // The segment's mousePressEvent (TitleBar.cpp:382-387) routes
        // anywhere-click in the disconnected state to rttClicked so a
        // single signal covers both "click for diagnostics" (when
        // connected) and "click to connect" (when disconnected). Branch
        // here on the live connection state to honor the "Click to
        // connect" affordance the segment paints — without this branch,
        // a disconnected click trapped the user in NetworkDiagnostics
        // instead of opening the connection panel (Codex P2 review
        // against PR #158, MainWindow.cpp:482).
        connect(seg, &ConnectionSegment::rttClicked, this, [this]() {
            const auto state = m_radioModel->connectionState();
            if (state == ConnectionState::Disconnected
                || state == ConnectionState::LinkLost) {
                showConnectionPanel();
                return;
            }
            auto* dlg = new NetworkDiagnosticsDialog(
                m_radioModel, m_radioModel->audioEngine(), this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
        connect(seg, &ConnectionSegment::audioPipClicked, this, [this]() {
            // Audio pip click also opens diagnostics — audio section
            // is the most relevant panel for pip trouble-shooting.
            auto* dlg = new NetworkDiagnosticsDialog(
                m_radioModel, m_radioModel->audioEngine(), this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
        connect(seg, &ConnectionSegment::contextMenuRequested,
                this, &MainWindow::showSegmentContextMenu);

        // 7. D.3: Hover tooltip — event filter delivers QHelpEvent.
        seg->setToolTip(QString());   // suppress Qt's own tooltip; we intercept
        seg->setAttribute(Qt::WA_AlwaysShowToolTips, false);
        seg->installEventFilter(this);

        // Seed with current state (Disconnected at launch).
        seg->setState(m_radioModel->connectionState());
    }

    buildStatusBar();
    applyDarkTheme();

    // Wire connection state changes to status bar
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

    // Issue #118 — show a transient status-bar message when a band-button
    // click short-circuits (locked slice, XVTR without transverter config).
    // Prevents silent failure — the user sees why nothing happened.
    connect(m_radioModel, &RadioModel::bandClickIgnored,
            this, [this](Band /*band*/, const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 3000);
    });

    // Phase 3Q Task 10: auto-connect failure / ambiguity surface.
    //
    // autoConnectFailed — the probe timed out or the radio rejected the
    // handshake. Open the ConnectionPanel, highlight the failed target,
    // and post an 8-second status-bar explanation.
    connect(m_radioModel, &RadioModel::autoConnectFailed,
            this, [this](const QString& mac, NereusSDR::ConnectFailure reason) {
        showConnectionPanel();
        if (m_connectionPanel) {
            m_connectionPanel->highlightMac(mac);
        }
        const auto saved = AppSettings::instance().savedRadio(mac);
        const QString name = (saved.has_value() && !saved->info.name.isEmpty())
            ? saved->info.name : mac;
        const QString reasonText =
            (reason == NereusSDR::ConnectFailure::Timeout)
                ? QStringLiteral("isn't reachable from this network")
                : QStringLiteral("returned an error");
        showToast(
            QStringLiteral("Auto-connect target %1 %2. Pick a different radio or update the address.")
                .arg(name, reasonText),
            ToastSeverity::Warning, 8000);
    });

    // autoConnectAmbiguous — multiple radios have AutoConnect = true.
    // The most-recently-connected MAC was chosen; post a 6-second advisory
    // pointing the user at Manage Radios for cleanup.
    connect(m_radioModel, &RadioModel::autoConnectAmbiguous,
            this, [this](int count, const QString& chosenMac) {
        const auto saved = AppSettings::instance().savedRadio(chosenMac);
        const QString name = (saved.has_value() && !saved->info.name.isEmpty())
            ? saved->info.name : chosenMac;
        showToast(
            QStringLiteral("%1 radios marked Auto-connect on launch. Using %2 (most recent). "
                           "Adjust in Manage Radios.")
                .arg(count).arg(name),
            ToastSeverity::Info, 6000);
    });

    // WDSP wisdom progress dialog — shown as a modal window during first-run
    // wisdom generation. Pattern from AetherSDR MainWindow::enableNr2WithWisdom().
    connect(m_radioModel->wdspEngine(), &WdspEngine::wisdomProgress,
            this, [this](int percent, const QString& status) {
        // Create dialog on first progress signal
        if (!m_wisdomDialog && percent < 100) {
            m_wisdomDialog = new QProgressDialog(this);
            m_wisdomDialog->setWindowTitle(QStringLiteral("NereusSDR — FFTW Wisdom"));
            m_wisdomDialog->setLabelText(
                QStringLiteral("Optimizing FFT plans for DSP engine...\n\n"
                               "This only happens on first run."));
            m_wisdomDialog->setRange(0, 100);
            m_wisdomDialog->setValue(0);
            m_wisdomDialog->setCancelButton(nullptr);
            m_wisdomDialog->setAutoClose(false);
            m_wisdomDialog->setMinimumWidth(500);
            m_wisdomDialog->setMinimumDuration(0);
            m_wisdomDialog->setWindowModality(Qt::ApplicationModal);
            m_wisdomDialog->setStyleSheet(QStringLiteral(
                "QProgressDialog { background: #0f0f1a; }"
                "QLabel { color: #c8d8e8; font-size: 13px; }"
                "QProgressBar {"
                "  text-align: center; font-size: 13px;"
                "  font-weight: bold; color: #c8d8e8;"
                "  background: #1a2a3a; border: 1px solid #2e4e6e;"
                "  border-radius: 3px; min-height: 24px;"
                "}"
                "QProgressBar::chunk { background: #00b4d8; }"));
            m_wisdomDialog->show();
        }

        if (m_wisdomDialog) {
            m_wisdomDialog->setValue(percent);
            if (!status.isEmpty() && percent < 100) {
                m_wisdomDialog->setLabelText(
                    QStringLiteral("Optimizing FFT plans for DSP engine...\n\n%1").arg(status));
            }
            if (percent >= 100) {
                m_wisdomDialog->setLabelText(QStringLiteral("FFTW planning complete!"));
                m_wisdomDialog->setValue(100);
                // Auto-close after brief delay
                QTimer::singleShot(800, this, [this]() {
                    if (m_wisdomDialog) {
                        m_wisdomDialog->close();
                        m_wisdomDialog->deleteLater();
                        m_wisdomDialog = nullptr;
                    }
                });
            }
        }
    });

    // Start discovery in background so radios are found before the user opens the panel
    m_radioModel->discovery()->startDiscovery();

    // Auto-reconnect to last radio — deferred so the event loop is running
    // before any signal/slot activity (e.g. discovery radioDiscovered).
    QTimer::singleShot(0, this, &MainWindow::tryAutoReconnect);

    // Phase 3J-2 + 3R M3: restore each spot client's auto-connect /
    // auto-start state. Sibling to tryAutoReconnect above; deferred via
    // singleShot(0, ...) so the QTcpSocket / QUdpSocket / QWebSocket
    // owned by each client see a fully-spun event loop before any
    // network I/O fires. Each client guards against double-start so
    // re-invocation is harmless (e.g. if a future code path also calls
    // this after a profile switch).
    //
    // 2026-05-18 bench fix: seed the SpectrumWidget per-source visibility
    // mask BEFORE restoring any auto-start state.  Without this, the
    // panadapter overlay mask (m_spotSourceVisible) is empty at startup
    // and the renderer's missing-key fallback (true == visible) leaks
    // every source's spots regardless of the user's "Show on panadapter"
    // checkbox state.  Bench operator hit it with FreeDV: FreeDvAutoStart
    // + Display checkbox unchecked = spots still painting at app start
    // until SpotHub was opened manually.  Six of seven sources share the
    // same symmetry break (PSK Reporter is send-only); fixing it here
    // closes the gap for all of them in one place.  The matching seed
    // inside openSpotHub() is now a defensive guard kept for the case
    // where the spectrum widget races construction; see lines below.
    QTimer::singleShot(0, this, [this] {
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->loadSpotDisplaySettings();
        }
        if (m_radioModel) {
            m_radioModel->restoreSpotClientAutoStartState();
        }
    });

    // Phase 3O Sub-Phase 11/12 — VAX first-run / startup rescan.
    // The Setup → Audio → VAX page (AudioVaxPage) is now live; users
    // who skip the first-run dialog can reach cable binding via
    // Setup → Audio → VAX at any time.
    // Deferred via singleShot(0, ...) so the UI is fully built first.
    QTimer::singleShot(0, this, &MainWindow::checkVaxFirstRun);

    // Task 23 — auto-open the Linux audio first-run dialog when no backend
    // is detected on this host. The check is deferred via singleShot(0, ...)
    // so the event loop is spinning before the modal dialog is posted.
    // The dialog's Dismiss button (Task 18) sets Audio/LinuxFirstRunSeen=True,
    // so this trigger fires at most once per user installation.
#if defined(Q_OS_LINUX)
    if (m_radioModel->audioEngine()->linuxBackend() == LinuxAudioBackend::None
        && AppSettings::instance().value(QStringLiteral("Audio/LinuxFirstRunSeen"),
                                          QStringLiteral("False")).toString()
               != QStringLiteral("True")) {
        QTimer::singleShot(0, this, &MainWindow::showAudioDiagnoseDialog);
    }
#endif

    // ── Phase 23: TCI Server instantiation ───────────────────────────────────
    // Phase 23 (bench fix 2026-05-10): TciServer instance is now created in
    // the constructor BEFORE buildUI (above) so populateDefaultMeter's applet
    // gate sees a non-null m_tciServer. Auto-start happens HERE (post-UI) so
    // serverStarted/serverStopped signals find the applets + indicator wired.
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer) {
        auto& s = AppSettings::instance();
        const bool enabled = s.value(QStringLiteral("TciServerEnabled"),
                                     QStringLiteral("False")).toString()
                             == QStringLiteral("True");
        const quint16 port = static_cast<quint16>(
            s.value(QStringLiteral("TciServerPort"),
                    QStringLiteral("50001")).toString().toUShort());
        // Phase 3J-1 closeout Item 1 (2026-05-12): bind-interface dropdown.
        // Default to loopback so a fresh install only exposes TCI to localhost;
        // operator opts into LAN exposure via Setup → CAT & Network → TCI Server.
        const QString bindStr = s.value(QStringLiteral("TciServerBindAddress"),
                                        QStringLiteral("127.0.0.1")).toString();
        QHostAddress bindAddr;
        if (!bindAddr.setAddress(bindStr)) {
            // Malformed AppSettings value — fall back to loopback rather than
            // refusing to start the server.  populateBindAddressCombo() ensures
            // only valid strings get persisted, but a hand-edited XML file
            // shouldn't crash startup.
            bindAddr = QHostAddress(QHostAddress::LocalHost);
        }
        if (enabled) {
            m_tciServer->start(bindAddr, port);
        }
    }
#endif

    // Defensive save on aboutToQuit. closeEvent is fine for ⌘Q but
    // does NOT run when the process is signaled (SIGTERM from pkill,
    // Activity Monitor force-quit, debugger detach). Without this
    // hook, any container/state change made mid-session is lost on
    // signal-based shutdown. saveState is idempotent so the ⌘Q path
    // (closeEvent → saveState; aboutToQuit → saveState again) is
    // harmless. (Preserved from main PR #13 alongside Phase 3I's
    // singleShot auto-reconnect above.)
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        // Same flush as closeEvent — covers the signal-shutdown path
        // (SIGTERM, force-quit, debugger detach) where closeEvent
        // doesn't run. Idempotent when closeEvent already flushed.
        m_shuttingDown = true;
        // 2026-05-22 bench-finding: graceful radio disconnect MUST happen
        // before the process tears down so the SendStop frame (run=0
        // CmdHighPriority) actually reaches the wire.  Without this,
        // pkill / SIGTERM / closing the window via the dock all skip
        // P2RadioConnection::disconnect, the radio gateware never sees
        // run=0, and lockups on the G2E require power-cycle.  Call
        // disconnectFromRadio FIRST so its 20 ms flush+sleep runs before
        // any other shutdown cleanup.
        if (m_radioModel) {
            m_radioModel->disconnectFromRadio();
            m_radioModel->flushPendingSettingsSave();
        }
        if (m_containerManager) {
            m_containerManager->saveState();
        }
        // Issue #206 — also flush window geometry on signal-based
        // shutdown (SIGTERM / force-quit). Idempotent with the
        // closeEvent path above.
        saveMainWindowGeometry();
        AppSettings::instance().save();
    });
}

MainWindow::~MainWindow() = default;

// Phase 3F Sub-Epic D Task 12: resolve the active pan's SpectrumWidget.
// Used as a backward-compat shim for call sites that still address "the"
// spectrum widget; long-term these should migrate to per-pan addressing.
// Returns nullptr if m_panStack isn't constructed yet (during early init)
// or if the active pan has no widget.
SpectrumWidget* MainWindow::activeSpectrumWidget() const
{
    if (!m_panStack) { return nullptr; }
    auto* applet = m_panStack->panadapter(m_panStack->activePanId());
    return applet ? applet->spectrumWidget() : nullptr;
}

// Phase 3F multi-pan: resolve the SpectrumWidget that owns this slice's
// panadapter from the slice's panKey(), falling back to the active pan when
// the key is empty (Slice A pre-seed) or the pan was removed.
// Ported from AetherSDR MainWindow::spectrumForSlice (MainWindow.cpp:14856
// [@6a142807]); AetherSDR uses s->panId() (a string), NereusSDR uses
// s->panKey().
SpectrumWidget* MainWindow::spectrumForSlice(SliceModel* s) const
{
    if (s && m_panStack) {
        if (auto* sw = m_panStack->spectrum(s->panKey())) {
            return sw;
        }
    }
    return activeSpectrumWidget();  // fallback to active pan
}

SliceModel* MainWindow::sliceForAddedIdForTest(RadioModel* model, int sliceId)
{
    return model ? model->sliceById(sliceId) : nullptr;
}

void MainWindow::applyAntennaChangeForTest(RadioModel* model, int sliceId,
                                            const QString& antennaName)
{
    if (SliceModel* slice = sliceForAddedIdForTest(model, sliceId)) {
        slice->setRxAntenna(antennaName);
    }
}

void MainWindow::wireRadeFlagForTest(RadioModel* model, VfoWidget* flag,
                                     int sliceId)
{
    if (!model || !flag) { return; }
    const QPointer<VfoWidget> flagRef(flag);
    connect(model, &RadioModel::radeSyncChanged, flag,
            [flagRef, sliceId](int changedSliceId, bool synced) {
        if (!flagRef || changedSliceId != sliceId) { return; }
        if (!synced) {
            flagRef->setRadeSynced(false);
        }
    });
    connect(model, &RadioModel::radeFreqOffsetChanged, flag,
            [flagRef, sliceId](int changedSliceId, float hz) {
        if (flagRef && changedSliceId == sliceId) {
            flagRef->setRadeFreqOffset(hz);
        }
    });
}

void MainWindow::configureSpectrumForPanForTest(SpectrumWidget* spectrum,
                                                 const QString& panId)
{
    if (!spectrum) { return; }
    bool ok = false;
    const int parsed = panId.startsWith(QStringLiteral("pan-"))
        ? panId.mid(4).toInt(&ok) : 0;
    spectrum->setPanIndex(ok && parsed >= 0 ? parsed : 0);
    spectrum->loadSettings();
}

void MainWindow::wireWidebandExtensionForTest(SpectrumWidget* spectrum,
                                              RadioModel* model,
                                              PanadapterStack* stack,
                                              const QString& panId)
{
    if (!spectrum || !model || !stack) { return; }
    const QPointer<RadioModel> modelRef(model);
    const QPointer<PanadapterStack> stackRef(stack);
    const auto resolve = [modelRef, stackRef, panId]() -> SliceModel* {
        if (!modelRef || !stackRef) { return nullptr; }
        PanadapterApplet* applet = stackRef->panadapter(panId);
        if (!applet) { return nullptr; }
        if (SliceModel* active =
                modelRef->sliceById(applet->activeSliceIndex())) {
            return active;
        }
        for (SliceModel* slice : modelRef->slices()) {
            if (slice
                && applet->associatedSlices().contains(slice->sliceIndex())) {
                return slice;
            }
        }
        return nullptr;
    };
    connect(spectrum, &SpectrumWidget::widebandExtensionStateChanged,
            spectrum, [resolve](bool on) {
        if (SliceModel* slice = resolve()) {
            slice->setWidebandExtensionRequested(on);
        }
    });
    connect(spectrum, &SpectrumWidget::ddcRetuneRequested,
            spectrum, [resolve](double freqHz) {
        if (SliceModel* slice = resolve()) {
            slice->setFrequency(freqHz);
        }
    });
    // Settings restore and rate seeding may have derived the actual state
    // before this bridge existed, so seed the resolved slice immediately.
    if (SliceModel* slice = resolve()) {
        slice->setWidebandExtensionRequested(spectrum->extendedMode());
    }
}

void MainWindow::fanWidebandBinsForTest(PanadapterStack* stack, int adcIndex,
                                        const QVector<float>& bins)
{
    if (!stack) { return; }
    for (PanadapterApplet* applet : stack->allApplets()) {
        if (applet && applet->spectrumWidget()) {
            applet->spectrumWidget()->setWidebandBins(adcIndex, bins);
        }
    }
}

// Phase 3F: create + fully wire a secondary slice's VfoWidget on the given
// SpectrumWidget. Factored out of the sliceAdded handler so the same wiring
// is reused on panKeyChanged migration. Slice A (index 0) keeps its own
// dedicated path in wireSliceToSpectrum(); this helper is for B+ flags.
//
// What's intentionally NOT done here: per-slice rxChannel/CTUN/MaxBin/NR/ANF
// DSP wiring (those hardcode rxChannel(0) today and are the actual Phase 3F
// multi-slice DSP epic). This is "make the flag appear + let the operator
// interact + keep it in sync with its SliceModel".
//
// Mirrors AetherSDR's addVfoWidget()+wireVfoWidget() pair (MainWindow.cpp:
// 11583 + 13968 [@6a142807]).
VfoWidget* MainWindow::createSliceFlag(SliceModel* slice, SpectrumWidget* sw)
{
    if (!slice || !sw || !m_radioModel) { return nullptr; }
    const int sliceIndex = slice->sliceIndex();
    if (m_vfoWidgetsBySlice.contains(sliceIndex)) {
        return m_vfoWidgetsBySlice.value(sliceIndex);
    }

    VfoWidget* newFlag = sw->addVfoWidget(sliceIndex);
    if (!newFlag) { return nullptr; }
    m_vfoWidgetsBySlice.insert(sliceIndex, newFlag);

    // Initial slice state push so the flag paints the right freq/mode/filter
    // on first show (mirrors wireSliceToSpectrum for Slice A).
    newFlag->setSlice(slice);
    newFlag->setFrequency(slice->frequency());
    // Seed the HOSTING widget's VFO marker too, not just the flag's own text.
    // These are separate state: the flag paints the digits, but the pan places
    // the flag from its own VFO frequency. setVfoFrequency was only ever
    // called on activeSpectrumWidget(), so a slice on any other pan left that
    // pan's marker at its 0 Hz default -- the flag was positioned off the left
    // edge and the pan showed the "<| 0.0000" off-screen-VFO chevron instead.
    // Bench-caught 2026-07-26: looked exactly like the flag was never created.
    sw->setVfoFrequency(slice->frequency());
    newFlag->setMode(slice->dspMode());

    // AUTO AGC-T on this flag. The toggle and its visual feedback were wired
    // only in wireSliceToSpectrum -- Slice A's path -- so the AUTO button on
    // every other flag did nothing when clicked and never lit. Bench-caught
    // 2026-07-26 on a four-slice layout.
    //
    // Noise floor comes from THIS slice's stream, matching the auto-AGC tick:
    // the visual would otherwise report stream 0's floor next to a threshold
    // computed from the slice's own.
    connect(newFlag, &VfoWidget::autoAgcToggled,
            slice, &SliceModel::setAutoAgcEnabled);
    connect(slice, &SliceModel::autoAgcEnabledChanged, this,
            [this, flagRef = QPointer<VfoWidget>(newFlag), slice](bool on) {
        if (!flagRef) { return; }
        NoiseFloorTracker* nft = m_radioModel->noiseFloorTrackerForSlice(slice);
        const float nf = nft ? nft->noiseFloor() : -200.0f;
        flagRef->updateAgcAutoVisuals(on, nf, slice->autoAgcOffset());
    });

    // Per-slice S-meter. The unqualified MeterPoller::smeterUpdated is wired
    // to Slice A's flag only (wireSliceToSpectrum), because the poller reads
    // one channel; every other flag's level bar sat dead. Filter the
    // slice-qualified signal for this flag's own slice.
    if (m_meterPoller) {
        const int myIdx = sliceIndex;
        connect(m_meterPoller, &MeterPoller::sliceSmeterUpdated, newFlag,
                [flagRef = QPointer<VfoWidget>(newFlag), myIdx](int idx, double dbm) {
            if (flagRef && idx == myIdx) { flagRef->setSmeter(dbm); }
        });
    }
    newFlag->setFilter(slice->filterLow(), slice->filterHigh());
    newFlag->setAgcMode(slice->agcMode());
    newFlag->setAfGain(slice->afGain());
    newFlag->setRfGain(slice->rfGain());
    newFlag->setRxAntenna(slice->rxAntenna());
    newFlag->setTxAntenna(slice->txAntenna());
    newFlag->setStepHz(slice->stepHz());
    newFlag->setBoardCapabilities(m_radioModel->boardCapabilities());
    newFlag->setHpsdrSku(m_radioModel->hardwareProfile().model);
    newFlag->setRxBypassActive(m_radioModel->alexController().rxOutOnTx());
    newFlag->setFilterPresetStore(m_radioModel->filterPresetStore());
    // Phase 3F closeout — give the per-slice VfoWidget the RadioModel pointer
    // so its right-click antenna submenu builds AntennaPickerMenu with live
    // caps + alex + slice (instead of the stub ANT1/ANT2 list).
    newFlag->setRadioModel(m_radioModel);
    wireRadeFlagForTest(m_radioModel, newFlag, sliceIndex);
    if (TxSliceArbiter* arb = m_radioModel->txSliceArbiter()) {
        newFlag->setTxSlice(arb->txBoundSliceId() == sliceIndex);
    }

    // --- Intent signals (Sub-Epic C T9 + Sub-Epic E T4 mirror) ---
    connect(newFlag, &VfoWidget::txHandoffRequested, this,
            [this](int idx) {
        if (m_radioModel && m_radioModel->txSliceArbiter()) {
            m_radioModel->requestTxHandoffToSlice(idx);
        }
    });
    // Phase 3F Sub-Epic I closeout, defect G2: route to the slice's DDC
    // stream. This used to write SliceModel::setSampleRateHz, which stopped
    // reaching the wire once buildStreamConfigsForCodec began sourcing the
    // rate from the allocator, so the menu did nothing. requestSliceSampleRate
    // also resolves by slice ID rather than list position, which is what
    // VfoWidget actually carries.
    connect(newFlag, &VfoWidget::sampleRateRequested, this,
            [this](int sliceId, int hz) {
        if (!m_radioModel) { return; }
        m_radioModel->requestSliceSampleRate(sliceId, hz);
    });
    connect(newFlag, &VfoWidget::filterPolicyRequested, this,
            [this](int chainIdx) {
        if (!m_radioModel) { return; }
        auto* alex = &m_radioModel->alexControllerMutable();
        FilterPolicyDialog dlg(chainIdx, alex, this);
        dlg.exec();
    });
    connect(newFlag, &VfoWidget::removeSliceRequested, this,
            [this](int idx) {
        if (m_radioModel) { m_radioModel->removeSlice(idx); }
    });
    // Phase 3F (Bug 2): the floating ✕ close button emits closeRequested.
    // Wire it to removeSlice so operators can dismiss a flag they opened.
    // (removeSliceRequested above is the right-click-menu path; this is the
    // dedicated button. Both land on RadioModel::removeSlice, which refuses
    // to remove the last remaining slice.)
    connect(newFlag, &VfoWidget::closeRequested, this,
            [this](int idx) {
        if (m_radioModel) { m_radioModel->removeSlice(idx); }
    });
    // Phase 3F (Bug 3): clicking this flag activates its slice so the RX
    // applet, the pan the flag sits on, and every other active-slice surface
    // follow it. Every flag runs through here, Slice A's included, since the
    // flag-path unification made createSliceFlag the one builder. Mirrors
    // AetherSDR's VfoWidget::sliceActivationRequested -> setActiveSlice
    // (wireVfoWidget, MainWindow.cpp:14076 [@6a142807]).
    //
    // setActiveSliceById, not setActiveSlice: VfoWidget carries the value
    // createSliceFlag stamped from SliceModel::sliceIndex(), a stable slice
    // ID, while setActiveSlice indexes m_slices positionally. With A(0) B(1)
    // C(2), closing B leaves C at id 2 / position 1, and the unconverted call
    // asked for position 2 of a two-element list -- so clicking flag C
    // selected nothing at all.
    connect(newFlag, &VfoWidget::sliceActivationRequested, this,
            [this](int sliceId) {
        if (m_radioModel) { m_radioModel->setActiveSliceById(sliceId); }
    });
    // Phase 3F closeout — AntennaPickerMenu selection forwards to
    // SliceModel::setRxAntenna. Sub-Epic E Task 5 consumer wire-up.
    connect(newFlag, &VfoWidget::antennaChangeRequested, this,
            [this](int idx, const QString& antName) {
        applyAntennaChangeForTest(m_radioModel, idx, antName);
    });

    // --- VfoWidget -> SliceModel (user click propagates to model) ---
    connect(newFlag, &VfoWidget::frequencyChanged, this,
            [slice](double hz) { slice->setFrequency(hz); });
    connect(newFlag, &VfoWidget::modeChanged, this,
            [slice](DSPMode mode) { slice->setDspMode(mode); });
    connect(newFlag, &VfoWidget::filterChanged, this,
            [slice](int low, int high) { slice->setFilter(low, high); });
    connect(newFlag, &VfoWidget::agcModeChanged, this,
            [slice](AGCMode mode) { slice->setAgcMode(mode); });
    connect(newFlag, &VfoWidget::afGainChanged, this,
            [slice](int gain) { slice->setAfGain(gain); });
    connect(newFlag, &VfoWidget::rfGainChanged, this,
            [slice](int gain) { slice->setRfGain(gain); });
    connect(newFlag, &VfoWidget::rxAntennaChanged, this,
            [slice](const QString& ant) { slice->setRxAntenna(ant); });
    connect(newFlag, &VfoWidget::txAntennaChanged, this,
            [slice](const QString& ant) { slice->setTxAntenna(ant); });

    // --- SliceModel -> VfoWidget (model updates repaint the flag) ---
    QPointer<VfoWidget> flagPtr(newFlag);
    connect(slice, &SliceModel::frequencyChanged, this,
            [this, flagPtr, slice](double hz) {
        if (flagPtr) { flagPtr->setFrequency(hz); }
        if (m_handlingBandJump) { return; }
        // Keep the hosting pan's VFO marker on this slice as it tunes.
        // Resolved per-call rather than captured, because a slice can migrate
        // to another pan and the flag follows it there.
        SpectrumWidget* host = spectrumForSlice(slice);
        if (!host) { return; }

        // Band jump: the slice moved outside what this pan is showing, so the
        // pan has to follow it. Restored after the flag-path unification
        // dropped the handler that carried it -- nothing set m_handlingBandJump
        // afterwards, so a slice tuned to another band left its pan behind.
        //
        // Per pan now, and using this slice's own WDSP channel rather than the
        // hardcoded rxChannel(0) the single-pan version used. The DDC retune
        // itself lives in RadioModel::bindSliceToStream (Sub-Epic I) and is not
        // duplicated here; this is the DISPLAY half.
        const double center = host->centerFrequency();
        const double halfBw = host->bandwidth() / 2.0;
        const bool offScreen = (hz < center - halfBw) || (hz > center + halfBw);
        if (!host->ctunEnabled() || offScreen) {
            m_handlingBandJump = true;
            const bool wasCtun = host->ctunEnabled();
            if (m_radioModel->receiverManager()) {
                m_radioModel->receiverManager()->setDdcFrequencyLocked(false);
            }
            host->setCenterFrequency(hz);
            host->setDdcCenterFrequency(hz);
            // Phase 3F Sub-Epic J Task 11: resolved through RadioModel's
            // accessor rather than wdspEngine()->rxChannel() directly --
            // src/gui/ no longer reaches into WdspEngine for a channel.
            if (RxChannel* ch = m_radioModel->rxChannelForSlice(slice->sliceIndex())) {
                ch->setShiftFrequency(0.0);
            }
            if (wasCtun && m_radioModel->receiverManager()) {
                m_radioModel->receiverManager()->setDdcFrequencyLocked(true);
            }
            m_handlingBandJump = false;
        } else {
            // CTUN, still on-screen: the DDC stays put and WDSP shifts.
            if (RxChannel* ch = m_radioModel->rxChannelForSlice(slice->sliceIndex())) {
                ch->setShiftFrequency(hz - center);
            }
        }
        host->setVfoFrequency(hz);
    });
    // Mode and filter drive the flag's LABELS and this pan's PASSBAND.
    //
    // The passband half was lost when the two flag-wiring paths were unified:
    // wireSliceToSpectrum had `activeSpectrumWidget()->setFilterOffset(low,
    // high)` and `setTxMode(mode)`, and the shared path only ever set the
    // flag's text -- so the shaded passband stopped following the filter and
    // never flipped to the other side of the dial on a USB/LSB change.
    // Bench-caught immediately, 2026-07-26.
    //
    // Resolved through spectrumForSlice per call, so the passband lands on the
    // pan hosting this slice rather than on whichever pan is active.
    connect(slice, &SliceModel::dspModeChanged, this,
            [this, flagPtr, slice](DSPMode mode) {
        if (flagPtr) { flagPtr->setMode(mode); }
        if (SpectrumWidget* host = spectrumForSlice(slice)) {
            // TX filter overlay maps audio Hz to the right sideband from this.
            host->setTxMode(mode);
            // Re-push the passband: the sideband, and therefore the sign of
            // the offsets, changes with the mode.
            host->setFilterOffset(slice->filterLow(), slice->filterHigh());
        }
    });
    connect(slice, &SliceModel::filterChanged, this,
            [this, flagPtr, slice](int low, int high) {
        if (flagPtr) { flagPtr->setFilter(low, high); }
        if (SpectrumWidget* host = spectrumForSlice(slice)) {
            host->setFilterOffset(low, high);
        }
    });
    connect(slice, &SliceModel::agcModeChanged, this,
            [flagPtr](AGCMode mode) {
        if (flagPtr) { flagPtr->setAgcMode(mode); }
    });
    connect(slice, &SliceModel::afGainChanged, this,
            [flagPtr](int gain) {
        if (flagPtr) { flagPtr->setAfGain(gain); }
    });
    connect(slice, &SliceModel::rfGainChanged, this,
            [flagPtr](int gain) {
        if (flagPtr) { flagPtr->setRfGain(gain); }
    });
    connect(slice, &SliceModel::stepHzChanged, this,
            [flagPtr](int hz) {
        if (flagPtr) { flagPtr->setStepHz(hz); }
    });
    connect(slice, &SliceModel::rxAntennaChanged, this,
            [flagPtr](const QString& ant) {
        if (flagPtr) { flagPtr->setRxAntenna(ant); }
    });
    connect(slice, &SliceModel::txAntennaChanged, this,
            [flagPtr](const QString& ant) {
        if (flagPtr) { flagPtr->setTxAntenna(ant); }
    });


    // ---- Moved from wireSliceToSpectrum (Slice A's private path) ----
    //
    // These were wired only for Slice A, so on every other flag Mute, BIN,
    // SQL, the AGC-T slider, Pan, RIT/XIT, NR, NB, SNB, ANF, APF, Lock and
    // the setup shortcuts did nothing at all: the flag never reached the
    // model, so the per-slice model->WDSP work could not help them.
    // createSliceFlag is now the single place a flag is wired.
    connect(newFlag, &VfoWidget::rxBypassToggled,
            &m_radioModel->alexControllerMutable(), &AlexController::setRxOutOnTx);
    connect(slice, &SliceModel::lastRadeRxCallsignChanged,
            newFlag, &VfoWidget::setRadeCallsign);
    connect(slice, &SliceModel::ritEnabledChanged, this, [newFlag](bool on) {
        newFlag->setRitEnabled(on);
    });
    connect(slice, &SliceModel::ritHzChanged, this, [newFlag](int hz) {
        newFlag->setRitHz(hz);
    });
    connect(slice, &SliceModel::xitEnabledChanged, this, [newFlag](bool on) {
        newFlag->setXitEnabled(on);
    });
    connect(slice, &SliceModel::xitHzChanged, this, [newFlag](int hz) {
        newFlag->setXitHz(hz);
    });
    connect(slice, &SliceModel::nbModeChanged, newFlag, &VfoWidget::setNbMode);
    newFlag->setNbMode(slice->nbMode());   // initial sync
    connect(slice, &SliceModel::snbEnabledChanged, this, [newFlag](bool v) {
        newFlag->setSnbEnabled(v);
    });
    connect(slice, &SliceModel::apfEnabledChanged, this, [newFlag](bool v) {
        newFlag->setApfEnabled(v);
    });
    connect(slice, &SliceModel::apfTuneHzChanged, this, [newFlag](int hz) {
        newFlag->setApfTuneHz(hz);
    });
    connect(newFlag, &VfoWidget::txFilterMatchRequested, this,
            [this](int audioLow, int audioHigh) {
        m_radioModel->transmitModel().setFilterLow(audioLow);
        m_radioModel->transmitModel().setFilterHigh(audioHigh);
    });
    connect(newFlag, &VfoWidget::nbModeCycled, this, [slice] {
        slice->setNbMode(NereusSDR::cycleNbMode(slice->nbMode()));
    });
    connect(newFlag, &VfoWidget::anfChanged, this, [slice](bool on) {
        slice->setAnfEnabled(on);
    });
    connect(newFlag, &VfoWidget::nr2Changed, this, [slice](bool on) {
        // NR2 = EMNR in Thetis naming. Toggle: NR2→active clears any other slot.
        slice->setActiveNr(on ? NereusSDR::NrSlot::NR2 : NereusSDR::NrSlot::Off);
    });
    connect(newFlag, &VfoWidget::snbChanged, this, [slice](bool on) {
        slice->setSnbEnabled(on);
    });
    connect(newFlag, &VfoWidget::apfChanged, this, [slice](bool on) {
        slice->setApfEnabled(on);
    });
    connect(newFlag, &VfoWidget::apfTuneHzChanged, this, [slice](int hz) {
        slice->setApfTuneHz(hz);
    });
    connect(slice, &SliceModel::mutedChanged, this, [newFlag](bool v) {
        newFlag->setMuted(v);
    });
    connect(slice, &SliceModel::audioPanChanged, this, [newFlag](double p) {
        newFlag->setAudioPan(p);
    });
    connect(slice, &SliceModel::ssqlEnabledChanged, this, [newFlag](bool v) {
        newFlag->setSsqlEnabled(v);
    });
    connect(slice, &SliceModel::ssqlThreshChanged, this, [newFlag](double d) {
        newFlag->setSsqlThresh(d);
    });
    connect(slice, &SliceModel::agcThresholdChanged, this, [newFlag](int v) {
        newFlag->setAgcThreshold(v);
    });
    connect(slice, &SliceModel::binauralEnabledChanged, this, [newFlag](bool v) {
        newFlag->setBinauralEnabled(v);
    });
    connect(newFlag, &VfoWidget::muteChanged, this, [slice](bool v) {
        slice->setMuted(v);
    });
    connect(newFlag, &VfoWidget::panChanged, this, [slice](double p) {
        slice->setAudioPan(p);
    });
    connect(newFlag, &VfoWidget::squelchEnabledChanged, this, [slice](bool v) {
        slice->setSsqlEnabled(v);
    });
    connect(newFlag, &VfoWidget::squelchThreshChanged, this, [slice](int v) {
        slice->setSsqlThresh(static_cast<double>(v));
    });
    connect(newFlag, &VfoWidget::agcThreshChanged, this, [slice](int v) {
        slice->setAgcThreshold(v);
    });
    connect(newFlag, &VfoWidget::binauralChanged, this, [slice](bool v) {
        slice->setBinauralEnabled(v);
    });
    connect(newFlag, &VfoWidget::quickModeRequested, this, [slice](int index) {
        // Quick-mode buttons: 0=USB, 1=CW, 2=DIG (matching AetherSDR defaults)
        static constexpr DSPMode kQuickModes[] = {DSPMode::USB, DSPMode::CWU, DSPMode::DIGU};
        if (index >= 0 && index < 3) {
            slice->setDspMode(kQuickModes[index]);
        }
    });
    connect(newFlag, &VfoWidget::openSetupRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("AGC/ALC"));
        dialog->show();
    });
    connect(newFlag, &VfoWidget::openNbSetupRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("NB/SNB"));
        dialog->show();
    });
    connect(newFlag, &VfoWidget::openNrSetupRequested, this,
            [this](NereusSDR::NrSlot slot) {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("NR/ANF"));
        // Deep-link to the sub-tab matching the NR slot the user clicked
        // (Task 18 polish 2026-04-23 — previously always opened NR1).
        if (auto* nrPage = dialog->findChild<NrAnfSetupPage*>()) {
            nrPage->selectSubtab(slot);
        }
        dialog->show();
    });
    connect(newFlag, &VfoWidget::ritEnabledChanged, this, [slice](bool on) {
        slice->setRitEnabled(on);
    });
    connect(newFlag, &VfoWidget::ritHzChanged, this, [slice](int hz) {
        slice->setRitHz(hz);
    });
    connect(newFlag, &VfoWidget::xitEnabledChanged, this, [slice](bool on) {
        slice->setXitEnabled(on);
    });
    connect(newFlag, &VfoWidget::xitHzChanged, this, [slice](int hz) {
        slice->setXitHz(hz);
    });
    connect(newFlag, &VfoWidget::stepCycleRequested, this, [slice]() {
        int current = slice->stepHz();
        int next = kStageOneStepLadder[0];
        for (int i = 0; i < kStageOneStepLadderSize; ++i) {
            if (kStageOneStepLadder[i] == current) {
                next = kStageOneStepLadder[(i + 1) % kStageOneStepLadderSize];
                break;
            }
        }
        // setStepHz emits stepHzChanged which the :1626-1629 handler uses to
        // propagate to activeSpectrumWidget()->setStepSize and newFlag->setStepHz.
        slice->setStepHz(next);
    });
    connect(newFlag, &VfoWidget::lockChanged, this, [slice](bool locked) {
        slice->setLocked(locked);
    });
    connect(slice, &SliceModel::lockedChanged, this, [newFlag](bool v) {
        newFlag->setLocked(v);
    });
    return newFlag;
}

// ── Phase 3F Sub-Epic I Task 8: one FFTEngine per DDC stream ────────────────
//
// The panadapter belongs to the DDC, not to the sub-receiver: ChannelMaster
// holds a single `volatile long run_pan` per `_rcvr` alongside
// `audio[cmMAXSubRcvr]` (cmaster.h:75-82 [v2.10.3.15]).  So slices that share
// a DDC share one spectrum and appear as separate flags on it, and the engine
// pool is sized by stream, not by slice.
FFTEngine* MainWindow::createFftEngineForStream(int streamIndex)
{
    if (streamIndex < 0) { return nullptr; }
    if (FFTEngine* existing = m_fftEngines.value(streamIndex, nullptr)) {
        return existing;
    }
    // m_fftThread must exist before an engine can be parked on it, and
    // m_radioModel before the I/Q feed can be wired; buildUI has both in
    // place ahead of the first createFftEngineForStream call.
    if (!m_fftThread || !m_radioModel) { return nullptr; }

    // No QObject parent: ownership is the deleteLater below, fired when the
    // FFT thread finishes.  Matches the pre-pool single-engine lifecycle.
    auto* engine = new FFTEngine(streamIndex);
    engine->setSampleRate(768000.0);
    engine->setFftSize(4096);

    auto& s = AppSettings::instance();
    const int persistedFps = qBound(1,
        s.value(QStringLiteral("DisplaySpectrumFps"),
                QStringLiteral("30")).toString().toInt(),
        60);
    engine->setOutputFps(persistedFps);

    const int persistedFftSize = s.value(
        QStringLiteral("DisplayFftSize"),
        QString::number(engine->fftSize())).toString().toInt();
    engine->setFftSizeBaseline(persistedFftSize);
    engine->setFftSize(persistedFftSize);

    const int defaultWin = static_cast<int>(engine->windowFunction());
    const int persistedWin = qBound(0,
        s.value(QStringLiteral("DisplayFftWindow"),
                QString::number(defaultWin)).toString().toInt(),
        static_cast<int>(WindowFunction::Count) - 1);
    engine->setWindowFunction(static_cast<WindowFunction>(persistedWin));

    engine->setHzPerBinTarget(
        s.value(QStringLiteral("DisplayHzPerBinTarget"),
                QStringLiteral("0")).toString().toDouble());

    engine->moveToThread(m_fftThread);
    connect(m_fftThread, &QThread::finished, engine, &QObject::deleteLater);

    // Raw I/Q for this stream -> this engine.  The context object is the
    // ENGINE, not MainWindow, deliberately: RadioModel emits
    // rawIqDataForStream from the Connection thread (Lever 2, 2026-05-24,
    // RadioModel.cpp Step 2a), and an engine-scoped connection resolves to a
    // queued delivery straight onto the FFT thread.  Routing through a
    // MainWindow-scoped lambda instead would put every I/Q packet through the
    // main thread's event loop (~3200/s per stream at 768 kHz), silently
    // undoing that fix.  The index filter costs one compare on the FFT
    // thread; the alternative -- reading m_fftEngines from the Connection
    // thread -- would need synchronisation AND lose Qt's automatic
    // disconnect-on-destroy, which is what makes this safe at shutdown.
    connect(m_radioModel, &RadioModel::rawIqDataForStream, engine,
            [engine, streamIndex](int idx, const QVector<float>& samples) {
        if (idx != streamIndex) { return; }
        engine->feedIQ(samples);
    });

    // Linear-power frame -> every pan subscribed to this stream.
    connect(engine, &FFTEngine::fftReadyLinear,
            this, &MainWindow::dispatchFftFrameToPans);

    // One NoiseFloorTracker per stream, fed by that stream's own FFT.
    //
    // Auto AGC-T derives its threshold from the noise floor, so a slice must
    // measure the band it is actually on. There used to be a single tracker
    // fed only by primaryFftEngine(), i.e. stream 0 -- fine while one slice
    // existed, but it would set a 20m slice's threshold from 40m's noise
    // floor once auto-AGC ran for every slice.
    auto* nf = new NoiseFloorTracker;
    m_streamNoiseFloors.insert(streamIndex, nf);
    if (m_radioModel) { m_radioModel->setStreamNoiseFloorTracker(streamIndex, nf); }
    connect(engine, &FFTEngine::fftReady, this,
            [nf](int, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nf->feed(binsDbm, kFrameIntervalMs);
    });

    m_fftEngines.insert(streamIndex, engine);
    return engine;
}

void MainWindow::applyStreamWindowToPan(const QString& panId, int streamIndex)
{
    if (!m_panStack) { return; }
    const auto it = m_streamWindows.constFind(streamIndex);
    if (it == m_streamWindows.constEnd()) { return; }
    SpectrumWidget* sw = m_panStack->spectrum(panId);
    if (!sw) { return; }
    sw->setDdcCenterFrequency(it->centreHz);
    if (it->sampleRateHz > 0) {
        sw->setSampleRate(static_cast<double>(it->sampleRateHz));
    }

    // Move the DISPLAY window onto the stream too, not just the DDC centre.
    //
    // setDdcCenterFrequency only tells the widget where the DDC sits for
    // bin-to-frequency mapping; the visible span is separate state, and a pan
    // created after startup keeps SliceModel's 14.225 MHz default. Bench-caught
    // 2026-07-26 on a 2v layout: pan-1 was correctly subscribed to a 7.265 MHz
    // stream while still displaying 14.2258 MHz, which
    //   - made visibleBinRange() select bins entirely outside the stream, so
    //     the waterfall rendered saturated (solid red), and
    //   - put the slice's flag at an x position far off the left edge, so the
    //     pan looked like it had no flag at all.
    //
    // Both symptoms are the same missing line. Recentre, preserving the pan's
    // current span so an operator's zoom is not thrown away -- only a pan that
    // has never been placed is actually moved, because pan-0 already sits on
    // its stream and this is a no-op there.
    // Span is clamped to the stream's own width. Preserving a wider one would
    // reintroduce the same failure at the edges: a pan left at the 192 kHz
    // default over a 48 kHz DDC has three quarters of its window outside the
    // data, which is exactly the out-of-range saturation this is fixing.
    const double streamWidthHz = static_cast<double>(it->sampleRateHz);
    double spanHz = sw->bandwidth();
    if (spanHz <= 0.0 || (streamWidthHz > 0.0 && spanHz > streamWidthHz)) {
        spanHz = streamWidthHz;
    }
    if (spanHz > 0.0) {
        sw->setFrequencyRange(it->centreHz, spanHz);
    }
}

// Phase 3F: WIDE badge fan-out. See RadioModel::panBypassState for the
// routing (pan -> slices -> stream -> ADC -> BpfEffective) and for the
// per-cause reason strings.
//
// Every pan is refreshed on every pass, not just the ones that changed. A
// bypass is a property of the CHAIN, so one slice crossing a band edge can
// flip the badge on pans that do not host it and never saw an event of
// their own. Rechecking all of them is one query per pan against
// single-digit slice and pan counts.
void MainWindow::refreshPanWideBadges()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const RadioModel::PanBypassState st =
            m_radioModel->panBypassState(applet->associatedSlices());
        applet->setWideBpf(st.bypassed, st.reason);
    }
}

// Phase 3F: status-overlay fan-out. Sibling of refreshPanWideBadges above,
// and deliberately the same shape: ask the model per pan, push the answer at
// that pan, never reach into the widget tree.
//
// Which slice a pan shows is its OWN activeSliceIndex, not the globally
// active slice -- per Sub-Epic E plan Task 2 Step 4. A pan hosting several
// slices has one of them active (PanadapterApplet::addSlice seeds it,
// removeSlice re-picks), and a global read would make every pan on a
// multi-pan layout paint identical text, which is the one thing a per-pan
// overlay exists not to do.
//
// updateStatusOverlay had zero callers before this, so every pan painted the
// widget's construction-time placeholders -- slice "A", "0.000", "USB",
// "CH 0" -- on a live radio.
void MainWindow::refreshPanStatusOverlays()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const int sliceId = applet->activeSliceIndex();
        SliceModel* slice = m_radioModel->sliceById(sliceId);
        // A pan with no slices keeps whatever it last painted rather than
        // being blanked: the operator is mid-drag between pans and a flash
        // to placeholder text reads as a fault.
        if (!slice) { continue; }
        applet->updateStatusOverlay(slice, m_radioModel->sliceChainIndex(sliceId));
    }
}

void MainWindow::wirePanStatusOverlayTriggers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        connect(applet, &PanadapterApplet::activeSliceChanged,
                this, &MainWindow::refreshPanStatusOverlays,
                Qt::UniqueConnection);
    }
}

// Phase 3F: badge-click fan-out. Third sibling of refreshPanWideBadges and
// wirePanStatusOverlayTriggers above, on the same hook and for the same
// reason: a pan that comes into existence after startup has to be wired
// without anyone remembering to wire it.
//
// The connects name member slots rather than lambdas on purpose. This runs
// again on every countChanged, so it has to be idempotent, and
// Qt::UniqueConnection is silently ignored for lambda targets in Qt6 -- a
// lambda here would stack one extra connection per layout switch and open
// that many FilterPolicyDialogs on a single click.
// ---------------------------------------------------------------------------
// ensureOverlayPanels — one control strip per panadapter.
//
// The strip used to be a single instance parented to pan-0's SpectrumWidget,
// so every other pan had no BAND / ANT / Display / +RX at all, and the one
// button that did exist had to guess which pan the operator meant. A control
// drawn on a pan acts on that pan: each strip now owns its panId and emits it,
// the same shape as AetherSDR's SpectrumOverlayMenu (SpectrumOverlayMenu.cpp:
// 292-315 [@c6481cbf], where +RX emits addRxClicked(m_panId)).
//
// Idempotent and re-armed from PanadapterStack::countChanged, so pans created
// by a layout switch or an Add Panadapter get their strip by construction
// rather than by remembering. Panels for pans that went away are dropped --
// the widget itself dies with its parent SpectrumWidget.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// wireSpectrumForPan — mouse interaction for one panadapter.
//
// Every SpectrumWidget signal used to be connected exactly once, to whatever
// activeSpectrumWidget() returned during startup -- pan-0's widget. The connect
// binds the OBJECT, not the expression, so it never re-resolved: on any other
// pan, click-to-tune, filter-edge drag, pan drag, zoom-replan and the dBm strip
// were all inert. The flag stayed live because it is wired per slice, which is
// why tuning appeared to work only with the pointer over the flag's digits.
//
// Resolves the slice per call through sliceForPan, so each pan drives its own
// slice, and uses that slice's WDSP channel rather than the hardcoded
// rxChannel(0) the single-pan path used.
// ---------------------------------------------------------------------------
// Push the live connection state into every pan's spectrum widget. Safe to
// call at any time; a pan created later is seeded by wireSpectrumForPan.
void MainWindow::pushConnectionStateToPans()
{
    if (!m_radioModel || !m_panStack) { return; }
    const ConnectionState st = m_radioModel->connectionState();
    const bool live = m_radioModel->isConnected();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        if (SpectrumWidget* sw = m_panStack->spectrum(applet->panId())) {
            sw->setConnectionState(st);
            // Clearing stale history on disconnect was pan-0 only, so every
            // other pan kept painting the waterfall it had when the radio went
            // away -- indistinguishable from live data.
            if (!live) { sw->clearWaterfallHistory(); }
        }
    }
}

void MainWindow::wireSpectrumForPan(SpectrumWidget* sw, const QString& panId)
{
    if (!sw || !m_radioModel) { return; }

    configureSpectrumForPanForTest(sw, panId);

    // Seed the new pan with the CURRENT connection state. Without this a pan
    // created after connect sits at the Disconnected default until the next
    // state change, and its disconnected guard swallows every mouse press.
    sw->setConnectionState(m_radioModel->connectionState());

    // Band plan too: setBandPlanManager was another activeSpectrumWidget()
    // one-shot, so the band-segment bar ("PHONE General" and friends) drew on
    // pan-0 only and every other pan showed a bare spectrum.
    sw->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

    // Right-click a spot on THIS pan to remove it. Was pan-0 only, so spots
    // could be dismissed from one pan and were inert on every other.
    if (SpotModel* spots = m_radioModel->spotModel()) {
        connect(sw, &SpectrumWidget::spotRemoveRequested,
                spots, &SpotModel::removeSpot);
    }

    // Hovering a spot on any pan drives the Spot Hub highlight.
    if (m_spotHubDialog) {
        connect(sw, &SpectrumWidget::spotHoverIndexChanged,
                m_spotHubDialog.data(),
                &SpotHubDialog::setHoveredPanadapterSpot);
    }

    // Clicking a disconnected pan opens the connection panel, from any pan.
    connect(sw, &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::showConnectionPanel);

    // CTUN max-bin offset follows THIS pan's slice rather than the globally
    // active one, so a max-bin readout on a background pan is not measured
    // against a slice sitting on some other pan's stream.
    connect(sw, &SpectrumWidget::ddcCenterFrequencyChanged, this,
            [this, panId](double ddcCenter) {
        if (!m_radioModel || !m_radioModel->wdspEngine()) { return; }
        if (SliceModel* s = sliceForPan(panId)) {
            m_radioModel->wdspEngine()->setMaxBinSliceOffsetHz(
                /*disp=*/0, s->frequency() - ddcCenter);
        }
    });

    // NOT wired here: bandwidthChangeRequested. Zoom itself already works on
    // every pan (SpectrumWidget narrows its own visible bin range), but the
    // auto-replan that keeps bins-per-pixel constant across zoom levels runs
    // against primaryFftEngine() -- one engine, pan-0's. Routing it per pan
    // needs the FFT engine looked up by the pan's stream, which is a bigger
    // change than this function. Until then a deep zoom on another pan stays
    // visually correct but does not gain FFT resolution.
}

// ---------------------------------------------------------------------------
// wireSpectrumSliceControls: the four spectrum controls that act on a slice.
//
// Bench 2026-07-28: "when I click to tune it always tunes flag A, not the last
// selected." Proven with per-hop instrumentation rather than by reading: a
// synthetic frequencyClicked on pan-0 fired exactly one handler, the copy in
// wireSliceToSpectrum, and sliceForPan was never called in the whole session.
//
// These four used to exist twice. wireSpectrumForPan held the per-pan version,
// and ensureOverlayPanels skips that function for pan-0 (its spot, connection
// and MaxBin hooks are wired elsewhere in this file and would double), so pan-0
// ran an older copy in wireSliceToSpectrum whose lambdas closed over
//
//     SliceModel* slice = m_radioModel->activeSlice();
//
// by value. connect() binds the object, and the lambda binds the pointer, so
// that copy was Slice A for the life of the session. On the single pan almost
// every operator uses, click-to-tune, the filter-edge drag, the pan drag and
// the CTUN toggle all drove Slice A and never consulted the pan at all. The
// per-pan active slice this epic added was correct and simply unread.
//
// One home now, called for every pan including pan-0, so the two cannot drift
// apart again. verify-no-captured-slice-spectrum-wiring.py fails the build if
// any of the four is connected to a sender other than `sw`; it is listed in
// the pre-commit hook and in CI's compliance job beside
// verify-no-gui-dsp-access.py, which guards the same class of mistake one
// layer down.
//
// Every handler resolves its slice through sliceForPan(panId) on each signal.
// That is the whole fix: resolving late is what lets the answer change when
// the operator selects a different flag.
// ---------------------------------------------------------------------------
void MainWindow::wireSpectrumSliceControls(SpectrumWidget* sw,
                                           const QString& panId)
{
    if (!sw || !m_radioModel) { return; }

    // Click on the spectrum tunes this pan's slice.
    connect(sw, &SpectrumWidget::frequencyClicked, this,
            [this, panId](double hz) {
        if (SliceModel* s = sliceForPan(panId)) { s->setFrequency(hz); }
    });

    // Drag a filter edge on this pan.
    connect(sw, &SpectrumWidget::filterEdgeDragged, this,
            [this, panId](int low, int high) {
        if (SliceModel* s = sliceForPan(panId)) { s->setFilter(low, high); }
    });

    // Drag the pan. Non-CTUN moves the VFO; CTUN pins the DDC to the pan centre
    // and offsets WDSP so audio stays on the VFO (Thetis radio.cs:1417 --
    // SetRXAShiftFreq receives +(freq - center)).
    connect(sw, &SpectrumWidget::centerChanged, this,
            [this, panId, sw](double centerHz) {
        if (m_handlingBandJump) { return; }
        SliceModel* s = sliceForPan(panId);
        if (!s) { return; }
        if (!sw->ctunEnabled()) {
            s->setFrequency(centerHz);
            return;
        }
        const int stream = s->streamIndex();
        if (stream >= 0 && m_radioModel->receiverManager()) {
            m_radioModel->receiverManager()->forceHardwareFrequency(
                stream, static_cast<quint64>(centerHz));
        }
        sw->setDdcCenterFrequency(centerHz);
        // Re-shift the WHOLE stream, not just this pan's slice. Addressing the
        // dragged slice's own channel fixed the hardcoded rxChannel(0), but a
        // shared DDC window still has one centre and N slices at their own
        // offsets inside it, so a co-host on the same stream kept the offset it
        // had before the drag and demodulated the wrong signal with its flag
        // still reading right.
        //
        // From Thetis radio.cs:1417 [v2.10.3.15]: SetRXAShiftFreq receives
        // +(freq - center). reshiftSlicesOnStream applies that per member.
        m_radioModel->reshiftSlicesOnStream(stream, centerHz);
    });

    // CTUN toggle restores this pan's whole stream rather than channel 0.
    //
    // Unpinning does not retune the DDC, so the members are still offset from
    // an unmoved centre; zeroing the shift would drop the demodulator onto the
    // DDC centre. Restore against where the DDC actually sits (the drag above
    // bypasses the allocator via forceHardwareFrequency and writes the centre
    // into this widget), and let the next VFO move settle the offsets to zero
    // through the now-unpinned allocator.
    connect(sw, &SpectrumWidget::ctunEnabledChanged, this,
            [this, panId, sw](bool enabled) {
        if (m_radioModel->receiverManager()) {
            m_radioModel->receiverManager()->setDdcFrequencyLocked(enabled);
        }
        if (enabled) { return; }
        if (SliceModel* s = sliceForPan(panId)) {
            m_radioModel->reshiftSlicesOnStream(s->streamIndex(),
                                                sw->ddcCenterFrequency());
        }
    });
}

// Keep the S-meter poller's channel list in step with the live slices.
//
// Slice id == WDSP RX channel id, so this is also the set of channels it reads
// for the per-slice pass that drives each flag's level bar.
//
// Hung off sliceAdded / sliceRemoved, NOT off the pan-count hook. It first
// lived inside ensureOverlayPanels, which only runs on
// PanadapterStack::countChanged -- so adding a slice to an EXISTING pan never
// refreshed the list and that slice's flag bar stayed dead. Bench-caught
// 2026-07-26.
void MainWindow::refreshMeterPollerSlices()
{
    if (!m_meterPoller || !m_radioModel) { return; }
    QList<int> ids;
    for (SliceModel* s : m_radioModel->slices()) {
        if (s) { ids << s->sliceIndex(); }
    }
    m_meterPoller->setSliceChannels(ids);
}

void MainWindow::ensureOverlayPanels()
{
    if (!m_panStack || !m_radioModel) { return; }

    refreshMeterPollerSlices();

    // Drop entries whose pan (and therefore whose parent widget) is gone.
    for (auto it = m_overlayPanels.begin(); it != m_overlayPanels.end(); ) {
        if (it.value().isNull() || !m_panStack->panadapter(it.key())) {
            it = m_overlayPanels.erase(it);
        } else {
            ++it;
        }
    }

    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const QString panId = applet->panId();
        if (m_overlayPanels.contains(panId)) { continue; }

        SpectrumWidget* sw = m_panStack->spectrum(panId);
        if (!sw) { continue; }

        // Same one-shot-per-pan guard as the strip below: this loop only runs
        // for pans that have no entry yet, so the interaction connects cannot
        // stack on a layout switch.
        //
        // The slice controls go to EVERY pan, pan-0 included. pan-0 used to be
        // excluded from the whole of wireSpectrumForPan and left running an
        // older copy of these four in wireSliceToSpectrum, whose lambdas held
        // a captured Slice A pointer -- which is the 2026-07-28 bench defect
        // where click-to-tune always retuned flag A. Those copies are gone, so
        // this is the only wiring for them and nothing doubles.
        wireSpectrumSliceControls(sw, panId);
        wireWidebandExtensionForTest(sw, m_radioModel, m_panStack, panId);

        if (panId != QStringLiteral("pan-0")) {
            // Still pan-0-excluded: its spot, connection and MaxBin hooks are
            // wired one-shot elsewhere in this file (the activeSpectrumWidget()
            // connects near the spot / ConnectionPanel / MaxBin sections), so
            // running them again here would double every one. Unifying those
            // the way the slice controls just were is a separate change with
            // its own regression surface.
            wireSpectrumForPan(sw, panId);
        }

        auto* panel = new SpectrumOverlayPanel(sw);
        panel->setPanId(panId);
        panel->setSliceResolver([this, panId]() {
            return sliceForPan(panId);
        });
        panel->move(4, 4);
        panel->show();
        m_overlayPanels.insert(panId, panel);

        // Phase 3O Sub-Phase 9 Task 9.2c — bind the VAX Ch combo to the model.
        panel->setRadioModel(m_radioModel);
        connect(applet, &PanadapterApplet::activeSliceChanged, panel,
                [panel](const QString&, int) {
            panel->bindToPanSlice();
        });

        // Phase 3P-I-a T18 — board caps drive the antenna combos, and are
        // re-pushed on every radio swap.
        panel->setBoardCapabilities(m_radioModel->boardCapabilities());
        connect(m_radioModel, &RadioModel::currentRadioChanged, panel,
                [this, panel]() {
            panel->setBoardCapabilities(m_radioModel->boardCapabilities());
        });

        // +RX adds a slice on the pan this strip belongs to. The id comes from
        // the signal, so this never consults an "active" pan.
        connect(panel, &SpectrumOverlayPanel::addRxClicked, this,
                [this](const QString& id) {
            if (!m_radioModel || id.isEmpty()) { return; }
            m_radioModel->addSliceOnPan(id);
        });

        // Band clicks act on this pan's active slice rather than the globally
        // active one, for the same reason (#118 fixed the mode-vs-frequency
        // half of this; the pan-targeting half arrives with per-pan strips).
        //
        // sliceIndex() is a stable slice ID, so it goes through
        // setActiveSliceById rather than the positional setActiveSlice --
        // otherwise a band click on a pan whose slice had a higher id than the
        // list is long (any mid-list removal) left the active slice where it
        // was and onBandButtonClicked retuned the wrong slice.
        connect(panel, &SpectrumOverlayPanel::bandSelected, this,
                [this, panId](const QString& name, double, const QString&) {
            if (!m_radioModel) { return; }
            if (SliceModel* s = sliceForPan(panId)) {
                m_radioModel->setActiveSliceById(s->sliceIndex());
            }
            m_radioModel->onBandButtonClicked(bandFromName(name));
        });

        // Pan-0's strip stays the one the display-settings wiring targets.
        if (m_overlayPanel == nullptr || panId == QStringLiteral("pan-0")) {
            m_overlayPanel = panel;
        }
    }
}

// The slice this pan hosts: its own active slice if it has one, else the first
// slice associated with it. Returns nullptr for a pan with no slices.
SliceModel* MainWindow::sliceForPan(const QString& panId) const
{
    if (!m_panStack || !m_radioModel) { return nullptr; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return nullptr; }
    const int active = applet->activeSliceIndex();
    if (active >= 0) {
        if (SliceModel* s = m_radioModel->sliceById(active)) { return s; }
    }
    for (SliceModel* s : m_radioModel->slices()) {
        if (s && applet->associatedSlices().contains(s->sliceIndex())) {
            return s;
        }
    }
    return nullptr;
}

void MainWindow::wirePanBadgeHandlers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        connect(applet, &PanadapterApplet::wideBadgeClicked,
                this, &MainWindow::onPanWideBadgeClicked,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::chainTagClicked,
                this, &MainWindow::onPanChainTagClicked,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::txBadgeClicked,
                this, &MainWindow::onPanTxBadgeClicked,
                Qt::UniqueConnection);
        // Phase 3F: clicking a pan makes it the active pan. Straight to the
        // stack's setter, exactly as AetherSDR MainWindow.cpp:12964 [@6a142807]
        // does it:
        //   connect(applet, &PanadapterApplet::activated,
        //           m_panStack, &PanadapterStack::setActivePan);
        // Member-pointer target on both ends, so Qt::UniqueConnection is
        // actually honoured here (it is silently dropped for lambdas -- see
        // RadioModel.cpp:5666), which matters because this re-runs on every
        // countChanged.
        connect(applet, &PanadapterApplet::activated,
                m_panStack, &PanadapterStack::setActivePan,
                Qt::UniqueConnection);
    }
}

int MainWindow::panChainIndex(const QString& panId) const
{
    if (!m_panStack || !m_radioModel) { return -1; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return -1; }

    const int chain = m_radioModel->sliceChainIndex(applet->activeSliceIndex());
    if (chain >= 0) { return chain; }

    // The slice is bound to no stream, so the model has no chain to give.
    // updateStatusOverlay holds the CH tag at its last known-good value in
    // exactly this case rather than claiming chain 0, so following it keeps
    // the dialog on the chain the operator can see. statusChainIndex() is the
    // read-back 0896b4f3 added for the overlay.
    return applet->statusChainIndex();
}

void MainWindow::onPanWideBadgeClicked(const QString& panId)
{
    onPanChainTagClicked(panId, -1);
}

// chainIdx is what the CH tag was painting when it was clicked; it is passed
// so the two entry points stay one function, but the live resolution wins.
// The tag is refreshed from the same call panChainIndex makes, so they agree
// in every case except a stale repaint, and the live answer is the honest one
// to open a dialog on.
void MainWindow::onPanChainTagClicked(const QString& panId, int chainIdx)
{
    if (!m_radioModel) { return; }

    int chain = panChainIndex(panId);
    if (chain < 0) { chain = chainIdx; }
    if (chain < 0) { return; }

    auto* alex = &m_radioModel->alexControllerMutable();
    FilterPolicyDialog dlg(chain, alex, this);
    dlg.exec();
}

// The TX pill hands the transmitter to the slice THIS pan is showing, which
// is the pan's own activeSliceIndex -- a slice ID, so it goes through
// RadioModel::requestTxHandoffToSlice, which converts to the list position
// TxSliceArbiter indexes by and drops MOX before flipping. The MOX drop stays
// the arbiter's; nothing here reproduces or bypasses it.
//
// Reachable only while this pan's slice already holds TX, because
// SpectrumStatusOverlay paints and hit-tests the pill on m_txBound. The
// handoff is therefore a no-op today, and is wired to the correct target so
// that it stays correct if the pill is ever given an unlit clickable state --
// a visual decision, not one to make here.
void MainWindow::onPanTxBadgeClicked(const QString& panId)
{
    if (!m_panStack || !m_radioModel) { return; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return; }
    m_radioModel->requestTxHandoffToSlice(applet->activeSliceIndex());
}

void MainWindow::wireSliceStatusOverlayTriggers(SliceModel* slice)
{
    if (!slice) { return; }

    // Connect the NOTIFY signal of every property the overlay reads, resolved
    // through the metaobject from the list PanadapterApplet publishes. Naming
    // the signals here instead would put the trigger set in a second place
    // that has to be remembered -- which is how updateStatusOverlay came to
    // have no callers at all.
    const QMetaObject* sliceMeta = slice->metaObject();
    const int refreshIdx =
        MainWindow::staticMetaObject.indexOfSlot("refreshPanStatusOverlays()");
    if (refreshIdx < 0) {
        // Renaming the slot without updating this string would otherwise
        // strand every overlay silently -- the exact failure this change
        // exists to fix. tst_pan_status_overlay pins the lookup so it cannot
        // reach a release, and this keeps a debug build loud if it ever does.
        qWarning("MainWindow: refreshPanStatusOverlays() slot not found; "
                 "per-pan status overlays will not follow slice state");
        return;
    }
    const QMetaMethod refresh = MainWindow::staticMetaObject.method(refreshIdx);

    for (const QByteArray& name : PanadapterApplet::statusOverlaySliceProperties()) {
        const int propIdx = sliceMeta->indexOfProperty(name.constData());
        if (propIdx < 0) { continue; }
        const QMetaProperty prop = sliceMeta->property(propIdx);
        if (!prop.hasNotifySignal()) { continue; }
        connect(slice, prop.notifySignal(), this, refresh, Qt::UniqueConnection);
    }
}

void MainWindow::rebuildFftRouting()
{
    if (!m_radioModel) { return; }

    // The WIDE badges ride this pass rather than getting their own connects
    // at each of the four call sites. The trigger set is identical -- any
    // change to which slices feed which pan through which stream moves both
    // answers -- so hanging the refresh here means every present and future
    // topology trigger reaches the badge by construction. Same reasoning the
    // Alex republish uses for hanging off bpfStateChanged (RadioModel.cpp:
    // 596-601). Ahead of the router guard on purpose: the badge is a model
    // question and stays correct with no FFTRouter present.
    refreshPanWideBadges();

    // The status overlay rides the same pass, for the same reason: every
    // topology trigger (slice add / remove, pan migration, stream rebind,
    // chain reassignment) moves which slice a pan shows and which chain feeds
    // it, so hanging the refresh here means present and future triggers reach
    // the overlay by construction rather than by remembering. The per-slice
    // frequency / mode triggers and each pan's activeSliceChanged are wired
    // separately, since those move the overlay without moving the topology.
    refreshPanStatusOverlays();

    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // Wholesale rebuild, not an incremental edit. A pan can host several
    // slices and FFTRouter::removePan drops a pan from EVERY receiver, so a
    // remove-then-add on one slice's migration would silently unsubscribe
    // its co-hosted neighbours. Binding signals also fire before sliceAdded
    // (plan discovery item 7), so only a rebuild-from-model consumer is
    // safe.
    //
    // Snapshot the pre-rebuild topology first so a brand-new subscription
    // can be told apart from one that merely survived. Only new ones get
    // the stream window pushed: streamBindingsChanged fires on every bind,
    // so on every VFO tick, and re-pushing the allocator's centre each time
    // would yank a CTUN pan back after an operator pan-drag (that path
    // retunes the DDC through forceHardwareFrequency without going through
    // the allocator).
    QHash<QString, QList<int>> before;
    if (m_panStack) {
        // PanadapterStack::allApplets (PanadapterStack.h:70) and
        // PanadapterApplet::panId (PanadapterApplet.h:65) both already exist.
        for (auto* applet : m_panStack->allApplets()) {
            if (!applet) { continue; }
            const QString panId = applet->panId();
            before.insert(panId, router->receiversForPan(panId));
            router->removePan(panId);
        }
    }

    for (SliceModel* slice : m_radioModel->slices()) {
        if (!slice) { continue; }
        const int stream = slice->streamIndex();
        if (stream < 0) { continue; }          // unbound slice feeds nothing

        // Resolving the pan is not just slice->panKey(). Slice A never has
        // one: RadioModel::addSlice stamps panKey only when it is given one
        // and connectToRadio calls the no-argument overload
        // (RadioModel.cpp:4137), so Slice A's panKey is permanently empty.
        // Skipping empties (as the Task 9 spec had it) would leave pan 0
        // with no subscription and no trace at all.
        //
        // Fall back to the pan that actually hosts the slice, recorded by
        // the sliceAdded handler through PanadapterApplet::addSlice. That
        // record is stable; activePanId() is not, and using it alone would
        // migrate Slice A's subscription (and darken pan 0) the moment the
        // operator made another pan active. activePanId() stays as the last
        // resort, matching spectrumForSlice (MainWindow.cpp:880).
        if (!m_panStack) { continue; }
        QString panId = slice->panKey();
        if (panId.isEmpty() || !m_panStack->panadapter(panId)) {
            panId.clear();
            for (auto* applet : m_panStack->allApplets()) {
                if (applet
                    && applet->associatedSlices().contains(slice->sliceIndex())) {
                    panId = applet->panId();
                    break;
                }
            }
        }
        if (panId.isEmpty()) { panId = m_panStack->activePanId(); }
        if (panId.isEmpty()) { continue; }

        const bool isNewSubscription = !before.value(panId).contains(stream);
        // mapPanToReceiver de-duplicates (FFTRouter.cpp:17), so two slices
        // sharing a stream and a pan produce one subscription, not two.
        router->mapPanToReceiver(panId, stream);
        if (isNewSubscription) {
            applyStreamWindowToPan(panId, stream);
        }
    }
}

void MainWindow::dispatchFftFrameToPans(int streamIndex,
                                        const QVector<float>& binsLinear,
                                        double windowEnb,
                                        double dbmOffset)
{
    if (!m_panStack || !m_radioModel) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // FFTRouter is the topology oracle rather than a signal hop:
    // pansForReceiver is public and unit-tested, and routing through its
    // own signal would add a queued hop on the render path for no gain.
    // One stream can feed N pans (different zoom levels of the same I/Q),
    // which is the AetherSDR overlay model the router was designed for.
    for (const QString& panId : router->pansForReceiver(streamIndex)) {
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            sw->updateSpectrumLinear(streamIndex, binsLinear,
                                     windowEnb, dbmOffset);
        }
    }
}

// Phase 3F Sub-Epic D Task 16: disconnect-before-removal for safe pan teardown.
// AetherSDR issue #242: deleting a widget with active connections to lambdas
// can race with queued signal delivery and crash. Disconnect first, then
// remove.
void MainWindow::disconnectPanadapter(const QString& panId)
{
    if (!m_panStack) { return; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return; }

    if (auto* sw = applet->spectrumWidget()) {
        sw->disconnect(this);
    }
    applet->disconnect(this);

    if (m_radioModel) {
        if (auto* router = m_radioModel->fftRouter()) {
            router->removePan(panId);
        }
    }
}

// Issue #206 — main-window geometry persistence. Qt's saveGeometry()
// returns a versioned QByteArray that already encodes position, size,
// AND window state (Normal/Maximized/FullScreen) plus screen identity
// for multi-monitor setups. We base64-encode it so AppSettings (which
// stores QString values) can round-trip the blob unmodified.
void MainWindow::saveMainWindowGeometry()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("MainWindowGeometry"),
               QString::fromLatin1(saveGeometry().toBase64()));
}

bool MainWindow::restoreMainWindowGeometry()
{
    auto& s = AppSettings::instance();
    const QString blob = s.value(QStringLiteral("MainWindowGeometry")).toString();
    if (blob.isEmpty()) {
        return false;
    }
    const QByteArray bytes = QByteArray::fromBase64(blob.toLatin1());
    if (bytes.isEmpty() || !restoreGeometry(bytes)) {
        return false;
    }

    // Multi-screen safety: if the restored frame's center sits outside
    // every connected screen (monitor disconnected since last save),
    // fall back to the centered 1280×800 default rather than parking
    // the window offscreen where the user can't reach it.
    const QPoint center = frameGeometry().center();
    if (!QGuiApplication::screenAt(center)) {
        resize(1280, 800);
        if (auto* primary = QGuiApplication::primaryScreen()) {
            const QRect avail = primary->availableGeometry();
            move(avail.center() - QPoint(width() / 2, height() / 2));
        }
        // Drop any saved Maximized/FullScreen bit so we don't immediately
        // re-maximize onto a screen layout that no longer matches.
        setWindowState(Qt::WindowNoState);
        return false;
    }

    return true;
}

void MainWindow::buildUI()
{
    // Title: name, version, then whatever else the operator needs to tell
    // this window apart from another one.
    //
    // Issue #100 added the active profile, so two instances against
    // different radios are distinguishable. The build tag is the same idea
    // for the same reason: a smoke build under test has to be
    // distinguishable from a release and from another worktree's build.
    // Standing rule (JJ, KG4VCF, 2026-07-30), earned when a session spent
    // real time proving by pgrep which binary was on screen.
    //
    // The tag is empty on release artifacts, which are built from a tag ref,
    // so their title stays exactly as it was. main() fills BuildIdentity in
    // from a header regenerated on every build, so the sha here is the sha
    // that was compiled, not the one that was current at the last configure.
    QString title = QStringLiteral("NereusSDR %1").arg(NEREUSSDR_VERSION);

    const QString buildTag = BuildIdentity::buildTag();
    if (!buildTag.isEmpty()) {
        title += QStringLiteral(" · %1").arg(buildTag);
    }

    // Profile stays last: it is per-run operator state, whereas the build
    // tag is a property of the binary, and the binary identity reads better
    // next to the version it belongs to.
    const QString profile = AppSettings::profileOverride();
    if (!profile.isEmpty()) {
        title += QStringLiteral(" [%1]").arg(profile);
    }

    setWindowTitle(title);
    setMinimumSize(800, 600);
    resize(1280, 800);

    // Issue #206 — restore last session's window position, size, and
    // maximized/fullscreen state. The 1280×800 above stays as the
    // first-launch fallback; restoreMainWindowGeometry() returns false
    // when no saved blob exists or the blob is corrupted, in which
    // case Qt centers the default size on the primary screen as before.
    restoreMainWindowGeometry();

    // --- Main QSplitter: spectrum (left) + container panel (right) ---
    // AetherSDR pattern: right panel is a proper layout element, not an overlay.
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #203040; }"));

    // Left side: spectrum + zoom bar
    auto* spectrumPane = new QWidget(m_mainSplitter);
    spectrumPane->setMinimumWidth(400);
    auto* layout = new QVBoxLayout(spectrumPane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Phase 3F Sub-Epic D Task 12: replace the single SpectrumWidget with
    // a PanadapterStack. PanadapterStack's constructor pre-creates
    // "pan-0" containing a PanadapterApplet whose embedded SpectrumWidget
    // becomes the new single-pan default. activeSpectrumWidget() resolves
    // to that widget for backward-compat call sites.
    m_panStack = new PanadapterStack(spectrumPane);
    m_panStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_panStack, 1);

    SpectrumWidget* const initialSpectrum = activeSpectrumWidget();
    if (initialSpectrum) {
        configureSpectrumForPanForTest(initialSpectrum,
                                       QStringLiteral("pan-0"));
    }

    // Task 13 wires per-pan rebinding when the active pan changes; for
    // now we just log/no-op.  Future polish: re-attach overlay panel,
    // peak-detector, spot bridges, etc. to the new active pan's widget.
    // Point the Setup dialog's Display pages at the pan the operator is on.
    //
    // Bench report 2026-07-30 (JJ, KG4VCF): "the second-N panadapters seem
    // not to honour the settings for display, pan 1 does, seems no way to
    // adjust the others."
    //
    // RadioModel::m_spectrumWidget is the single hook every Display setup
    // page pushes through (82 call sites across DisplaySetupPages,
    // AppearanceSetupPages and SpectrumPeaksPage). It was assigned once
    // during wiring from activeSpectrumWidget() and never again, so it kept
    // whichever widget happened to be active at startup for the whole
    // session. Every display control in Setup therefore acted on one pan and
    // silently did nothing for the others.
    //
    // Following the active pan is the smallest thing that makes the controls
    // reachable at all, and it suits the per-pan model: SpectrumWidget
    // already persists every display preference under
    // settingsKey(key, m_panIndex), so the pans genuinely have their own
    // settings and Setup just needs to say which one it means. Selecting a
    // pan and opening Setup now adjusts that pan.
    //
    // Not an activePanId indirection of the kind that rule forbids: this is
    // a global dialog choosing a target, not a control drawn on one pan
    // reaching sideways into another. A per-pan selector inside Setup, the
    // way Thetis splits RX1 and RX2 display settings, is the fuller answer.
    connect(m_panStack, &PanadapterStack::activePanChanged, this,
            [this](const QString& panId) {
        if (!m_radioModel || !m_panStack) { return; }
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            m_radioModel->setSpectrumWidget(sw);
            // R1 Task 4: RadioModel's own DSP-facing calls (SWR overlay,
            // applyClaritySmoothDefaults) go through the abstract
            // ISpectrumSink pointer, not m_spectrumWidget, so it has to
            // follow the active pan too. Same object, set from the same
            // sw at the same time; only the static type differs.
            m_radioModel->setSpectrumSink(sw);
        }
    });

    // Phase 3F: the status overlay's non-slice trigger. countChanged fires
    // from addPanadapter / removePanadapter, including the ones applyLayout
    // makes when the operator switches template, so this catches every pan
    // that comes into existence after startup -- which is every pan except
    // pan-0. Re-arming is safe: the connects inside are UniqueConnection.
    connect(m_panStack, &PanadapterStack::countChanged, this, [this](int) {
        wirePanStatusOverlayTriggers();
        wirePanBadgeHandlers();
        ensureOverlayPanels();
        refreshPanStatusOverlays();
    });

    // The S-meter poller's slice list keys off SLICE lifetime, not pan count.
    // Adding a slice to an existing pan moves no pan count, so hanging this on
    // countChanged alone left the new slice's flag bar dead.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int) { refreshMeterPollerSlices(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int) { refreshMeterPollerSlices(); });
    wirePanStatusOverlayTriggers();
    wirePanBadgeHandlers();

    // ── Bench 2026-07-28: click-to-tune always tuned flag A ────────────────
    //
    // The pan a slice lives on has to follow the operator's selection.
    // RadioModel::activeSlice() (global) and
    // PanadapterApplet::activeSliceIndex() (per pan) are independent, and
    // only the global one had a writer once addSlice had seeded the pan --
    // so a pan latched onto the first slice added to it and stayed there.
    // MainWindow::sliceForPan reads the per-pan value, which is what
    // click-to-tune, the filter-edge drag, the CH tag and the pan TX pill
    // all act on, so selecting flag B moved every global surface and left
    // all four still driving A. This is the missing writer.
    //
    // Direction is one way, pan follows global, and deliberately so:
    //
    //   * Every route the operator has for selecting a slice (a VfoWidget
    //     flag press, an RxApplet slice tab, a band button, TCI) already
    //     lands on RadioModel::setActiveSlice, so following it here covers
    //     all of them in one wire instead of one per entry point.
    //   * The reverse -- a pan's own re-pick promoting itself to the global
    //     active slice -- is NOT wired. PanadapterApplet::removeSlice
    //     re-picks silently when a pan loses its active slice, and letting a
    //     background pan's bookkeeping steal the RX applet, the container
    //     S-meter and the DSP menu out from under the operator is a change
    //     to what they see, not to what tunes. RadioModel::removeSlice
    //     already re-seats the global active slice on its own when the
    //     removed one held it.
    //
    // Only the pan hosting the slice moves; setActiveSliceOnHostingPan is
    // where that rule lives. activeSliceChanged carries a LIST POSITION, so
    // the id is resolved through activeSlice()->sliceIndex() rather than
    // used as it arrives.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int) {
        if (!m_panStack || !m_radioModel) { return; }
        if (SliceModel* active = m_radioModel->activeSlice()) {
            m_panStack->setActiveSliceOnHostingPan(active->sliceIndex());
        }
    });

    // Phase 3F Sub-Epic D Task 15: restore persisted pan layout + splitter
    // sizes. Reads PanLayoutId from AppSettings (default "1") and asks the
    // stack to materialise that many pans before restoring per-splitter
    // QByteArray state. Operators get their last layout back on launch.
    {
        auto& s = AppSettings::instance();
        const QString restoredLayout = s.value(QStringLiteral("PanLayoutId"),
                                                QStringLiteral("1")).toString();
        // Shares the template table with applyPanLayout, but deliberately not
        // the rest of it: this runs at startup, before a radio is connected
        // and before any slice exists, so there is nothing to rehome and the
        // slice add-loop would manufacture a slice pre-connect.
        // (Codex review round 3, PR #293.)
        m_panStack->applyLayout(restoredLayout, panIdsForLayout(restoredLayout));
        m_panStack->restoreSplitterState();

        // ...and finish the job once there IS a radio. Skipping the slice
        // add-loop above is right, but nothing used to pick it up afterwards,
        // so a persisted 2v layout came back with pan-1 permanently dead: no
        // trace, no waterfall, a 0.0000 flag, and no way forward except
        // noticing you have to add Slice B by hand (bench, 2026-08-01,
        // J.J. Boyd KG4VCF).
        //
        // Queued so it lands after the connect handlers that size the stream
        // pool and bind Slice A have run; addSliceOnPan needs a sized pool to
        // bind what it creates.
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState s) {
            if (s != ConnectionState::Connected) { return; }
            QMetaObject::invokeMethod(this, [this]() { populateEmptyPans(); },
                                      Qt::QueuedConnection);
        });
    }

    // Phase 3F Sub-Epic E Task 3: the badge clicks are armed for every pan
    // from the countChanged hook above, alongside the status-overlay
    // triggers. See wirePanBadgeHandlers.

    // Left overlay panel (SpectrumOverlayPanel) — child of the active
    // pan's SpectrumWidget. Construction is deferred when the active
    // pan has no widget (shouldn't happen because the stack ctor
    // creates pan-0, but be defensive).
    // One strip per pan, created here for the pans that exist at startup and
    // re-armed from the countChanged hook for every pan created later.
    // m_overlayPanel stays pointing at pan-0's so the display-settings and
    // band wiring further down keeps a stable target.
    ensureOverlayPanels();

    // ── 2026-05-12 bench fix: SpotModel → SpectrumWidget bridge ───────────
    //
    // The missing bridge — every spot ingestion path (Cluster / RBN /
    // WSJT-X / SpotCollector / POTA / FreeDV / PSK Reporter) lands in
    // RadioModel's per-source `on<Source>SpotReceived` slot which calls
    // `m_spotModel->applySpotStatus(...)`.  SpotModel then emits
    // spotAdded / spotUpdated / spotRemoved / spotsCleared.
    //
    // But upstream AetherSDR's `refreshSpots()` lambda on MainWindow —
    // the thing that translates SpotData into SpectrumWidget::SpotMarker
    // and calls `setSpotMarkers(...)` to repaint the panadapter overlay —
    // never carried over in the port.  Result: SpotTableModel (the Spot
    // List tab) fills correctly because it has its own per-source
    // wireClient lambda; the panadapter, however, has been blind to
    // every spot since Phase 3J-2 shipped.
    //
    // This block rebuilds the marker vector on every SpotModel change
    // (full rebuild, not delta — the typical spot map is <500 entries
    // with a 30-min lifetime).  DxCC-aware coloring uses the same
    // DxccColorProvider the SpotTableModel queries.
    if (auto* spotModel = m_radioModel->spotModel()) {
        auto refreshSpots = [this]() {
            if (!activeSpectrumWidget() || !m_radioModel) { return; }
            auto* spotModel = m_radioModel->spotModel();
            if (!spotModel) { return; }
            auto* dxccColor = m_radioModel->dxccColorProvider();

            const auto& spots = spotModel->spots();
            QVector<SpectrumWidget::SpotMarker> markers;
            markers.reserve(spots.size());
            for (auto it = spots.constBegin(); it != spots.constEnd(); ++it) {
                const SpotData& s = it.value();
                SpectrumWidget::SpotMarker m;
                m.index            = s.index;
                m.callsign         = s.callsign;
                // Prefer the RX (heard-on) frequency so cluster spots
                // land on the actual TX freq of the DX station.  Fall
                // back to txFreqMhz when only one is populated (e.g.
                // POTA / RBN sometimes ship rxFreqMhz only).
                m.freqMhz          = (s.rxFreqMhz > 0.0)
                                         ? s.rxFreqMhz
                                         : s.txFreqMhz;
                m.color            = s.color;
                m.mode             = s.mode;
                m.source           = s.source;
                m.spotterCallsign  = s.spotterCallsign;
                m.comment          = s.comment;
                m.timestampMs      = s.timestamp.isValid()
                                         ? s.timestamp.toMSecsSinceEpoch()
                                         : QDateTime::currentMSecsSinceEpoch();
                if (dxccColor && dxccColor->isEnabled()
                    && !m.callsign.isEmpty() && m.freqMhz > 0.0) {
                    m.dxccColor = dxccColor->colorForSpot(
                        m.callsign, m.freqMhz, m.mode);
                }
                markers.append(m);
            }
            activeSpectrumWidget()->setSpotMarkers(markers);
        };

        connect(spotModel, &SpotModel::spotAdded,
                this, [refreshSpots](const SpotData&) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotUpdated,
                this, [refreshSpots](const SpotData&) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotRemoved,
                this, [refreshSpots](int) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotsCleared,
                this, refreshSpots);
        connect(spotModel, &SpotModel::spotsRefreshed,
                this, refreshSpots);

        // 2026-05-12 bench fix (Gap #3 follow-on).  Right-click → Remove
        // Spot on the panadapter emits spotRemoveRequested(idx) → purge
        // the spot from SpotModel → spotRemoved fires → refreshSpots
        // above repaints the overlay without it.
        connect(activeSpectrumWidget(), &SpectrumWidget::spotRemoveRequested,
                spotModel, &SpotModel::removeSpot);
    }

    // Zoom slider bar below spectrum
    auto* zoomBar = new QSlider(Qt::Horizontal, spectrumPane);
    zoomBar->setRange(1, 768);
    zoomBar->setValue(768);
    zoomBar->setFixedHeight(20);
    zoomBar->setToolTip(QStringLiteral("Zoom: drag to adjust spectrum bandwidth"));
    zoomBar->setStyleSheet(QStringLiteral(
        "QSlider { background: #0a0a14; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 14px; margin: -4px 0; border-radius: 7px; }"));
    layout->addWidget(zoomBar);
    connect(zoomBar, &QSlider::valueChanged, this, [this](int val) {
        double bwHz = val * 1000.0;
        activeSpectrumWidget()->setFrequencyRange(activeSpectrumWidget()->centerFrequency(), bwHz);
        emit activeSpectrumWidget()->bandwidthChangeRequested(bwHz);
    });

    m_mainSplitter->addWidget(spectrumPane);

    // Right side: Container #0 will be added by ContainerManager
    setCentralWidget(m_mainSplitter);

    // --- Container Infrastructure (Phase 3G-1) ---
    m_containerManager = new ContainerManager(spectrumPane, m_mainSplitter, this);

    // Phase 3P-I-a T17 — push board caps into every container so
    // AntennaButtonItems gate their click handler on hasAlex. Re-runs
    // when the active radio changes (currentRadioChanged) and also fires
    // when a new container is added (containerAdded). Without this,
    // freshly-created or restored containers keep the default
    // m_hasAlex=true and would allow clicks on HL2/Atlas.
    auto pushCapsToAllContainers = [this]() {
        const auto caps = m_radioModel->boardCapabilities();
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            c->setBoardCapabilities(caps);
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            pushCapsToAllContainers);

    // Per-SKU power-meter rescale.  Bench-reported #167 follow-up: the
    // top MeterPanel BarItem stack ships with a 0-120 W default that
    // makes HL2 (5 W) a sliver and ANAN-G2-1K (1000 W) saturate.  When
    // the active radio changes, ask every MeterWidget to rescale its
    // PowerBar / PowerScale pair to the new SKU's PA ceiling.  Same
    // paMaxWattsFor() helper TxApplet uses for its RF Pwr HGauge so
    // both meter surfaces share a single source of truth.
    auto rescaleAllPowerMeters = [this]() {
        const HPSDRModel m = m_radioModel->hardwareProfile().model;
        const int maxW     = paMaxWattsFor(m);
        if (m_meterWidget) {
            m_meterWidget->rescalePowerMeters(maxW);
        }
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            for (MeterWidget* mw : c->findChildren<MeterWidget*>()) {
                mw->rescalePowerMeters(maxW);
            }
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            rescaleAllPowerMeters);

    // Phase 3F Sub-Epic D Task 11: gate the bottom-bar CH 1 stacked
    // indicator widget on whether the connected radio drives a second RX
    // filter chain. buildStatusBar() ships chain1Widget hidden by default;
    // this slot toggles it on/off as the user switches radios.
    //
    // Defect D4: this used to read adcCount, which names the wrong thing. The
    // indicator reports a CHAIN's band-pass state, and ANAN-100D / ANAN-200D
    // have two ADCs behind one filter bank: the setAlex2HPF model list at
    // Thetis console.cs:15435-15443 [v2.10.3.15] never hands them a second
    // filter word. Showing CH 1 there offered the operator a second chain to
    // reason about, and a Filter Policy override to set on it, that the radio
    // does not have.
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    auto updateChain1Visibility = [this]() {
        if (!m_chain1IndicatorWidget) { return; }
        const auto caps = m_radioModel->boardCapabilities();
        m_chain1IndicatorWidget->setVisible(caps.rxFilterChainCount >= 2);
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            updateChain1Visibility);

    // Phase 3F Sub-Epic D Task 11: drive the CH 0 / CH 1 bottom-bar
    // indicator text + colour from AlexController's per-ADC BPF state.
    // Filtered = green; WidebandLocked / Bypass = amber. reasonText
    // (set by AlexController::recomputeBpf) drives the body label.
    connect(&m_radioModel->alexController(),
            &AlexController::bpfStateChanged, this,
            [this](int adc, const AlexController::AlexAdcState& state) {
                auto* lbl = findChild<QLabel*>(
                    QStringLiteral("chainIndicator%1").arg(adc));
                if (!lbl) { return; }
                lbl->setText(state.reasonText);
                const QString color =
                    (state.effective == AlexController::BpfEffective::Filtered)
                        ? Style::kGreenText
                        : Style::kAmberWarn;
                lbl->setStyleSheet(
                    QStringLiteral("color: %1; font-size: 9px; font-weight: bold;")
                        .arg(color));
            });

    // Phase 3F: and drive the per-pan WIDE pill from the same signal.
    //
    // A separate connect rather than a line inside the lambda above: the two
    // surfaces answer different questions. The bottom-bar label reports one
    // chain by number, and this reports every pan that chain happens to feed,
    // which on a multi-pan layout is what tells the operator WHICH of their
    // receivers is exposed.
    //
    // The signal's own adc argument is deliberately unused. A recompute on
    // chain 1 can leave a pan straddling both chains bypassed for a reason
    // that now belongs to chain 0, so the refresh re-asks per pan rather than
    // trying to patch only the pans on the chain that moved.
    //
    // This covers the state-change half of the trigger set (band crossings,
    // wideband toggles, Filter Policy edits); rebuildFftRouting covers the
    // topology half (slice add / remove / pan migration / stream rebind).
    connect(&m_radioModel->alexController(),
            &AlexController::bpfStateChanged, this,
            [this](int, const AlexController::AlexAdcState&) {
                refreshPanWideBadges();
            });

    // Issue #118 — helper: wire a container's bandClicked signal through
    // the RadioModel handler. Invoked from the containerAdded callback,
    // which fires for every container materialized by ContainerManager
    // (runtime-added, restoreState(), and createDefaultContainers()).
    auto wireContainerBandClick = [this](ContainerWidget* c) {
        if (!c) { return; }
        connect(c, &ContainerWidget::bandClicked, this, [this](int idx) {
            m_radioModel->onBandButtonClicked(bandFromUiIndex(idx));
        });
    };

    connect(m_containerManager, &ContainerManager::containerAdded, this,
            [this, wireContainerBandClick](const QString& id) {
        if (auto* c = m_containerManager->container(id)) {
            c->setBoardCapabilities(m_radioModel->boardCapabilities());
            wireContainerBandClick(c);
        }
    });

    // Create the MeterPoller BEFORE restoreState / populateDefaultMeter
    // so the meterReadyForPolling signal fires into a live poller as
    // each container's MeterWidget is materialized. Previously the
    // poller was created later and only the panel container's
    // m_meterWidget was registered manually — every user-created
    // container's meter sat orphaned and bars never received setValue()
    // calls, the root of the "BarMeter not drawing" symptom.
    m_meterPoller = new MeterPoller(this);
    // Task 3.1: expose MeterPoller via RadioModel so MultimeterPage can
    // apply live interval + averaging-window changes without a MainWindow
    // round-trip.  Non-owning; RadioModel stores the pointer only.
    m_radioModel->setMeterPoller(m_meterPoller);
    // Task 3.2: expose ContainerManager via RadioModel so MultimeterPage
    // can broadcast unit-mode changes to all live MeterItems.
    m_radioModel->setContainerManager(m_containerManager);
    connect(m_containerManager, &ContainerManager::meterReadyForPolling,
            this, [this](MeterWidget* meter) {
        if (!meter || !m_meterPoller) { return; }
        m_meterPoller->addTarget(meter);
        // Auto-unregister when the meter is destroyed (e.g.
        // ContainerWidget::setContent deleteLater()s a previous
        // content during a swap). Without this the poller would
        // dereference a dangling pointer on its next tick.
        connect(meter, &QObject::destroyed, m_meterPoller,
                [this](QObject* obj) {
            if (m_meterPoller) {
                m_meterPoller->removeTarget(static_cast<MeterWidget*>(obj));
            }
        });
    });

    m_containerManager->restoreState();
    if (m_containerManager->containerCount() == 0) {
        createDefaultContainers();
    }
    // Always populate the panel container's content (meters + applets).
    // On first run, createDefaultContainers() creates the shell; on restore,
    // restoreState() recreates the shell but content is lost. This ensures
    // the applet panel is always populated regardless of restore path.
    populateDefaultMeter();
    m_containerManager->restoreSplitterState();

    // Phase 3P-I-a T17 — initial push. `containerAdded` fires during
    // restoreState() but the content (and any AntennaButtonItems) are
    // installed after the signal by populateDefaultMeter() or the saved
    // content factory. Do a one-shot sweep here so the final items
    // pick up the startup board capabilities.
    pushCapsToAllContainers();

    // Default splitter sizes on first run: ~80% spectrum, ~20% panel
    if (!AppSettings::instance().contains(QStringLiteral("MainSplitterSizes"))) {
        m_mainSplitter->setSizes({1024, 256});
    }

    // Wire spectrum display to SliceModel (values come from persisted state,
    // no longer hardcoded). Connection is deferred to wireSliceToSpectrum()
    // which runs after RadioModel creates slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index == 0) {
            wireSliceToSpectrum();
        }
    });

    // Push restored slice state into spectrum + VFO views once
    // RadioModel::loadSliceState() completes.
    //
    // wireSliceToSpectrum() above runs at sliceAdded() time — BEFORE
    // loadSliceState() runs inside connectToRadio.  At that earlier moment
    // the slice still holds its pre-restore default values, so the
    // spectrum widget's m_ddcCenterHz, center freq, and VFO freq were
    // seeded with the default (typically 14.225 MHz / 20m).  After
    // loadSliceState() restores the persisted band/freq/mode/filter,
    // SliceModel::frequencyChanged is gated by qFuzzyCompare and by the
    // CTUN-shift branch in wireSliceToSpectrum's frequencyChanged lambda
    // (the offScreen path that calls setDdcCenterFrequency only fires
    // when persisted freq is OUTSIDE default ±halfBw).  Without an
    // explicit re-push, m_ddcCenterHz stays at the default and spectrum
    // bin labels point to the wrong band of RF until the first dial-
    // tune crosses the offScreen threshold.
    //
    // Mirrors Thetis txtVFOAFreq_LostFocus's unconditional Display.VFOA
    // / Display.CentreFreqRX1 push at console.cs:31272 + 15378
    // [v2.10.3.13], invoked from chkPower_CheckedChanged at
    // console.cs:27204 [v2.10.3.13] as the explicit "push state to
    // display" step on power-on.
    connect(m_radioModel, &RadioModel::sliceStateRestored, this,
            [this](int index) {
        if (index != 0 || !activeSpectrumWidget()) {
            return;
        }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) {
            return;
        }
        const double freq = slice->frequency();
        activeSpectrumWidget()->setCenterFrequency(freq);
        activeSpectrumWidget()->setDdcCenterFrequency(freq);
        activeSpectrumWidget()->setVfoFrequency(freq);
        activeSpectrumWidget()->setFilterOffset(slice->filterLow(), slice->filterHigh());
        if (m_vfoWidget) {
            m_vfoWidget->setFrequency(freq);
            m_vfoWidget->setMode(slice->dspMode());
            m_vfoWidget->setFilter(slice->filterLow(), slice->filterHigh());
        }
    });

    // Phase 3Q Sub-PR-6 (F.1): bind RxDashboard to slice(0) once it exists.
    // buildStatusBar() runs before connectToRadio() so slices().isEmpty() at
    // construction time; defer binding to sliceAdded.
    // Resolved by id, not list position. sliceAdded carries the stable slice
    // id and addSlice reuses the lowest free one, so after Slice A is closed
    // and a new slice takes id 0 it is APPENDED to m_slices -- id 0 at some
    // other position. slices().at(0) then bound the dashboard to whichever
    // slice happened to be first. Same class of defect as the one closed in
    // requestSliceSampleRate (Sub-Epic I closeout, defect G2).
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int sliceId) {
        if (sliceId == 0 && m_rxDashboard) {
            if (SliceModel* s = m_radioModel->sliceById(0)) {
                m_rxDashboard->bindSlice(s);
            }
        }
    });

    // Phase 3F: the status overlay's per-slice triggers. Topology changes
    // reach the overlay through rebuildFftRouting, but a retune or a mode
    // change moves no topology at all, so those need their own wire -- and
    // they are the ones an operator exercises on every VFO detent.
    //
    // Resolved by id, not list position: sliceAdded carries the stable slice
    // id (RadioModel.cpp addSlice hands out the lowest free one), which is a
    // position only until the operator removes a slice from the middle.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int sliceId) {
        wireSliceStatusOverlayTriggers(m_radioModel->sliceById(sliceId));
        refreshPanStatusOverlays();
    });
    // Slices that already exist. RadioModel creates Slice A at connect, so on
    // a reconnect this loop is what re-arms it; sliceAdded has already fired.
    for (SliceModel* existing : m_radioModel->slices()) {
        wireSliceStatusOverlayTriggers(existing);
    }

    // Phase 3F Sub-Epic D Task 13: bind newly-created slices to a pan
    // and register pan-to-receiver routing in the FFTRouter.
    //
    // RadioModel::addSlice stamps the requested pan id on the SliceModel via
    // setPanKey() (and the transitional initialPanId property) so this handler
    // knows where to dock the slice. If the slice was added by some other path
    // (no pan key), we fall back to the active pan. If the requested pan does
    // not exist yet (e.g. Add Panadapter ran between addSliceOnPan emit and
    // this slot), we create it on the fly via PanadapterStack::addPanadapter.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int sliceIndex) {
        if (!m_panStack) { return; }
        SliceModel* slice = sliceForAddedIdForTest(m_radioModel, sliceIndex);
        if (!slice) { return; }

        const QString panKey = slice->panKey();
        const QString targetPan = panKey.isEmpty()
                                      ? m_panStack->activePanId()
                                      : panKey;

        auto* applet = m_panStack->panadapter(targetPan);
        if (!applet) {
            applet = m_panStack->addPanadapter(targetPan);
        }
        if (applet) {
            applet->addSlice(sliceIndex);
        }

        // Phase 3F Sub-Epic I Task 9: re-derive the whole topology instead
        // of mapping this one pan. The old call keyed the router on
        // slice->ddcIndex(), which is the hardware DDC number (2..6 on
        // Saturn-class), while the FFTEngine pool is keyed on stream index
        // (0..userDdcCount-1) -- plan invariant 3. It also ran too early for
        // Slice A: the pool is not sized until connect, so Slice A's
        // addSlice-time bind is a no-op and its streamIndex is still -1 here.
        // The streamBindingsChanged rebuild picks it up once it binds.
        rebuildFftRouting();

        // Phase 3F hotfix 2026-05-27: create a per-slice VfoWidget so
        // operators can see + interact with the new slice.  The existing
        // wireSliceToSpectrum() above runs for slice 0 (Slice A) and
        // creates m_vfoWidget; Slice B+ never had a flag, so multi-slice
        // was invisible.  We add the UI surface here and wire the same
        // intent signals (Sub-Epic C Task 9 + Sub-Epic E Task 4) plus
        // the bidirectional freq/mode/filter/AGC/gain/antenna/step state
        // signals so the new flag stays in sync with its SliceModel and
        // user clicks on the new flag propagate back to the model.
        //
        // What's intentionally NOT done here: per-slice rxChannel/CTUN/
        // MaxBin/NR/ANF DSP wiring (those hardcode rxChannel(0) today
        // and are the actual Phase 3F multi-slice DSP epic).  This hotfix
        // is "make the flag appear + let the operator interact"; the
        // DSP-side per-slice fanout is the next sub-epic.
        if (sliceIndex == 0) {
            // Slice A is handled by wireSliceToSpectrum() which already
            // ran via the index==0 handler at the top of this file.
            return;
        }
        if (m_vfoWidgetsBySlice.contains(sliceIndex)) {
            return;
        }
        // Phase 3F: route the new flag to the SpectrumWidget that hosts the
        // slice's pan (resolved from slice->panKey() by spectrumForSlice),
        // NOT the active pan. This is Bug 1's fix: previously the flag landed
        // on the active pan and stacked on pan-0 instead of the pan showing
        // the slice's band. Ported from AetherSDR's sliceAdded path
        // (MainWindow.cpp:11581 [@6a142807]).
        SpectrumWidget* sw = spectrumForSlice(slice);
        if (!sw) { return; }
        createSliceFlag(slice, sw);

        // Phase 3F: migrate the flag when the slice's owning pan changes.
        // Remove the VfoWidget from every pan's SpectrumWidget, then re-add
        // + re-wire it on the new pan resolved by spectrumForSlice(). This
        // is the panKeyChanged half of Bug 1's fix (the sliceAdded path
        // above handles initial placement). Ported from AetherSDR's
        // panIdChanged migration (MainWindow.cpp:11560 [@6a142807]).
        connect(slice, &SliceModel::panKeyChanged, this,
                [this, slice](const QString& newPanKey) {
            const int idx = slice->sliceIndex();
            // The pan-to-slice association has to move with the flag. Without
            // this the pan the slice left kept listing it in
            // associatedSlices() and could keep it as that pan's active
            // slice, so the old pan went on tuning, and painting the CH tag
            // for, a slice now shown somewhere else. Ahead of the flag
            // rebuild's early return below so the association is corrected on
            // any path that reaches this handler.
            if (m_panStack) {
                m_panStack->moveSliceToPan(idx, newPanKey);
            }
            if (idx == 0) { return; }  // Slice A flag is the dedicated path
            if (m_panStack) {
                for (auto* applet : m_panStack->allApplets()) {
                    if (applet && applet->spectrumWidget()) {
                        applet->spectrumWidget()->removeVfoWidget(idx);
                    }
                }
            }
            m_vfoWidgetsBySlice.remove(idx);
            SpectrumWidget* dest = spectrumForSlice(slice);
            if (dest) { createSliceFlag(slice, dest); }
            // Phase 3F Sub-Epic I Task 9: the slice now feeds a different
            // pan, so the FFT topology has to follow. Unconditional: the
            // model changed even when the destination pan does not exist
            // yet and no flag could be built.
            rebuildFftRouting();
        });
    });

    // Phase 3F Sub-Epic D Task 13: detach a removed slice index from
    // every pan. The associatedSlices set is small (single-digit) so a
    // linear scan across all applets is fine.
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int sliceIndex) {
        if (m_panStack) {
            for (auto* applet : m_panStack->allApplets()) {
                if (applet) { applet->removeSlice(sliceIndex); }
            }
        }
        // Phase 3F hotfix 2026-05-27: remove the per-slice VfoWidget
        // when its slice goes away.  Slice A's m_vfoWidget is owned by
        // SpectrumWidget via Qt parent and tied to the singleton slice
        // lifecycle; the cleanup path for it is fragile (m_vfoWidget is
        // referenced by lambdas captured in wireSliceToSpectrum), so
        // skip it here.  Only secondary slice flags are torn down.
        //
        // Phase 3F (Bug 1 follow-up): the flag may live on a non-active pan
        // now that flags route to spectrumForSlice(). Remove from EVERY pan's
        // SpectrumWidget (removeVfoWidget on a pan that doesn't host it is a
        // no-op) instead of just the active pan, which would leak the flag.
        if (sliceIndex != 0 && m_vfoWidgetsBySlice.contains(sliceIndex)) {
            VfoWidget* victim = m_vfoWidgetsBySlice.take(sliceIndex);
            if (victim && victim != m_vfoWidget) {
                bool removed = false;
                if (m_panStack) {
                    for (auto* applet : m_panStack->allApplets()) {
                        if (applet && applet->spectrumWidget()) {
                            applet->spectrumWidget()->removeVfoWidget(sliceIndex);
                            removed = true;
                        }
                    }
                }
                if (!removed) {
                    // Same orphan hazard as removeVfoWidget: the flag's
                    // floating buttons belong to the SpectrumWidget, so they
                    // outlive a bare delete and stay painted on the pan.
                    victim->destroyFloatingButtons();
                    victim->deleteLater();
                }
            }
        }
        // Phase 3F Sub-Epic I Task 9: the removed slice may have been the
        // last one feeding its pan, or the last one on its stream. Rebuild
        // from the surviving slice set rather than unsubscribing this pan,
        // which would also drop any co-hosted slices still showing on it.
        rebuildFftRouting();
    });

    // Phase 3F Sub-Epic F Task 6: route wideband bins from RadioModel into
    // the active pan's SpectrumWidget.  RadioModel emits one
    // widebandSpectrumReady per ADC per assembled frame; we forward the
    // bins to the currently-active SpectrumWidget which silently stores
    // them per ADC.  Per-pan ADC routing (so extended-pan views on the
    // correct ADC's bins) lands in Sub-Epic F polish (T7-T10).
    connect(m_radioModel, &RadioModel::widebandSpectrumReady, this,
            [this](int adcIdx, const QVector<float>& dbmBins) {
        fanWidebandBinsForTest(m_panStack, adcIdx, dbmBins);
    });

    // Create the FFTEngine pool on a worker thread (spectrum thread from
    // architecture).  Sample rate starts at P2 default (768k);
    // RadioModel::wireSampleRateChanged updates it to the actual wire rate on
    // each connect (P1=192k, P2=768k).
    //
    // Phase 3F Sub-Epic I Task 8: the thread is created BEFORE the engines
    // now, because createFftEngineForStream parks each engine on it.  Only
    // stream 0's engine is built here: the SKU's stream count is not known
    // until connect, and the rest are built on demand as the allocator
    // claims their DDCs (see the streamCentreChanged handler below).
    m_fftThread = new QThread(this);
    m_fftThread->setObjectName(QStringLiteral("SpectrumThread"));
    createFftEngineForStream(0);

    connect(m_radioModel, &RadioModel::wireSampleRateChanged,
            this, [this](double rateHz) {
        // Every stream on a P1 radio shares the wire rate, and on P2 the
        // per-stream rate published by streamCentreChanged is derived from
        // the same value, so the whole pool follows this signal.
        for (FFTEngine* engine : std::as_const(m_fftEngines)) {
            if (!engine) { continue; }
            QMetaObject::invokeMethod(engine, [engine, rateHz]() {
                engine->setSampleRate(rateHz);
            });
        }
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setSampleRate(rateHz);
            // Phase 3G-12: preserve the user's current zoom level across
            // sample rate changes. Only reset the visible span if the
            // current bandwidth would now exceed the new DDC sample rate
            // (in which case we clamp to full-span).
            const double freq = activeSpectrumWidget()->centerFrequency();
            const double currentBw = activeSpectrumWidget()->bandwidth();
            const double clampedBw = (currentBw > rateHz) ? rateHz : currentBw;
            activeSpectrumWidget()->setFrequencyRange(freq, clampedBw);
        }
    });
    // Spectrum/waterfall FPS — load persisted value (default 30), apply to
    // BOTH the FFT engine (row production cadence) and the SpectrumWidget
    // display timer (paint cadence) so the two stay locked.  Without this
    // load, FFTEngine defaulted to 30 and SpectrumWidget defaulted to its
    // own 30 fps constructor value, but Setup -> Display -> Spectrum
    // changes did not survive restart.  Persistence write side is in
    // SpectrumDefaultsPage::pushFps.
    //
    // Phase 3F Sub-Epic I Task 8: the engine half of this (setOutputFps),
    // plus the persisted FFT size / window / Hz-per-bin target that used to
    // follow it, now live in createFftEngineForStream so every pooled engine
    // comes up on the same display knobs.  Only the SpectrumWidget half is
    // left here.
    {
        const int persistedFps = qBound(1,
            AppSettings::instance().value(
                QStringLiteral("DisplaySpectrumFps"),
                QStringLiteral("30")).toString().toInt(),
            60);
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setDisplayFps(persistedFps);
        }
    }

    // 2026-05-25 KG4VCF bench fix: elevate the spectrum FFT thread.
    // 2026-05-26 KG4VCF revisit: bumped from USER_INITIATED to
    // USER_INTERACTIVE -- the INITIATED tier still let compile workers
    // preempt the FFT pass during build kickoff, producing visible
    // waterfall stutter even though the WaterfallTicker had its own
    // worker thread.  USER_INTERACTIVE puts the FFT thread in the
    // same scheduling class as audio + GUI so compile workers (DEFAULT)
    // consistently lose the time-slice race.  See
    // src/core/audio/RealtimeAudioPriority.h.
    // The lambda elevates the thread it runs on, so it is thread-scoped, not
    // engine-scoped: one connection using stream 0's engine as context is
    // enough for the whole pool.  Engines created later (at connect, when the
    // SKU's stream count is known) land on an already-elevated thread.
    connect(m_fftThread, &QThread::started, primaryFftEngine(),
            []() { NereusSDR::elevateLatencyCriticalThreadPriority(); });

    // Per-engine cleanup (QThread::finished -> deleteLater) and the raw I/Q
    // feed are wired inside createFftEngineForStream, so engines added after
    // this point get both.
    //
    // The linear-power frame no longer goes straight to activeSpectrumWidget()
    // -- that resolved to pan 0 permanently and was the reason a secondary pan
    // never animated.  Each engine's fftReadyLinear now lands in
    // MainWindow::dispatchFftFrameToPans, which consults the FFTRouter and
    // pushes the frame to every pan subscribed to that stream.  fftReadyLinear
    // carries the raw |X[k]|² bins plus windowEnb + dbmOffset metadata so the
    // detector + avenger pipeline reproduces the legacy fftReady dBm output
    // (FFTEngine.cpp:348 [v2.10.3.13]) at display-pixel resolution.  fftReady
    // (full-bin dBm) is kept as a separate signal for chrome / AGC consumers
    // (ClarityController, NoiseFloorTracker) wired below; those stay on the
    // primary engine because each is a single global consumer.

    // Phase 3G-8: expose view hooks on RadioModel so Display setup pages can
    // reach the renderer / FFT engine without depending on MainWindow.
    m_radioModel->setSpectrumWidget(activeSpectrumWidget());
    // R1 Task 4: same object as the line above, fed to the abstract
    // ISpectrumSink pointer RadioModel's own DSP-facing calls use. See the
    // activePanChanged handler earlier in this file for the matching
    // re-wire when the active pan changes.
    m_radioModel->setSpectrumSink(activeSpectrumWidget());
    m_radioModel->setFftEngine(primaryFftEngine());

    // Phase 3F Sub-Epic I Task 8: follow each stream's DDC centre + rate.
    //
    // RadioModel::bindSliceToStream emits this whenever the allocator claims
    // or moves a DDC.  The pans showing the stream must recentre with it, or
    // SpectrumWidget::visibleBinRange maps the incoming bins against a stale
    // window.  Also cached, because at emit time the router may not yet know
    // which pan shows the stream (see m_streamWindows in the header).
    connect(m_radioModel, &RadioModel::streamCentreChanged, this,
            [this](int streamIndex, double centreHz, int sampleRateHz) {
        m_streamWindows.insert(streamIndex, StreamWindow{centreHz, sampleRateHz});
        // This signal is emitted exactly when the allocator claims or moves
        // a DDC, which is the only way a stream starts producing I/Q, so it
        // is also the right moment to build that stream's engine. No-op
        // after the first time.
        if (FFTEngine* engine = createFftEngineForStream(streamIndex)) {
            QMetaObject::invokeMethod(engine, [engine, sampleRateHz]() {
                engine->setSampleRate(static_cast<double>(sampleRateHz));
            }, Qt::QueuedConnection);
        }
        if (m_radioModel) {
            if (auto* router = m_radioModel->fftRouter()) {
                for (const QString& panId : router->pansForReceiver(streamIndex)) {
                    applyStreamWindowToPan(panId, streamIndex);
                }
            }
        }
    });

    // Phase 3F Sub-Epic I Task 9: any change to which slices sit on which
    // stream re-derives the FFT topology. This is the authoritative trigger:
    // it fires from bindSliceToStream AFTER SliceModel::streamIndex is set,
    // including for Slice A's deferred bind at connect (the pool is not
    // sized when Slice A is created, so its addSlice-time bind is a no-op
    // and the sliceAdded rebuild sees streamIndex == -1).
    connect(m_radioModel, &RadioModel::streamBindingsChanged, this,
            [this](int, const QVector<int>&) { rebuildFftRouting(); });

    // Sub-epic E: flush the rewind ring buffer when the radio disconnects so
    // a new session starts with a clean history. AetherSDR's clearDisplay()
    // did this implicitly; NereusSDR has no equivalent single-call reset, so
    // we plumb the connection-state signal through here. See
    // docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md task 4.
    connect(m_radioModel, &RadioModel::connectionStateChanged, activeSpectrumWidget(),
            [this]() {
        if (!m_radioModel->isConnected() && activeSpectrumWidget()) {
            activeSpectrumWidget()->clearWaterfallHistory();
        }
    });

    // Phase 3Q-8: clicking the spectrum while disconnected opens ConnectionPanel.
    connect(activeSpectrumWidget(), &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::showConnectionPanel);

    // Wire BandPlanManager → SpectrumWidget so the bandplan strip renders on launch.
    activeSpectrumWidget()->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

    // Phase 3G-9b: no first-launch auto-apply of smooth defaults. Per
    // user decision 2026-04-15, the out-of-box waterfall stays on
    // WfColorScheme::Default; ClarityBlue is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage or by
    // manually selecting "Clarity Blue" from the Waterfall Defaults combo.

    // --- Phase 3G-13: Step attenuator + ADC overload ---
    m_stepAttController = new StepAttenuatorController(this);
    m_radioModel->setStepAttController(m_stepAttController);

    // 3M-1a G.1 / F.2: MoxController::hardwareFlipped → StepAttenuatorController.
    // Both objects are now live; RadioModel owns MoxController, MainWindow owns
    // StepAttenuatorController. Wire here where both sides are accessible.
    // Qt::QueuedConnection documents cross-component intent (both main-thread)
    // and ensures the slot body runs after the emit call stack unwinds.
    // F.2 connect note: this is the connect deferred from StepAttenuatorController.h
    // line 257 ("The connect() call wiring this slot to MoxController::hardwareFlipped
    // is deferred to Task G.1").
    // From Thetis console.cs:29546-29576 [v2.10.3.13] — ATT-on-TX in HdwMOXChanged.
    // Inline attribution tags preserved verbatim from the cited range:
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps  [console.cs:29561]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567]
    //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value  [console.cs:29568]
    // Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29576]
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::hardwareFlipped,
                m_stepAttController, &StepAttenuatorController::onMoxHardwareFlipped,
                Qt::QueuedConnection);

        // H.1 (Phase 3M-1a): SpectrumWidget MOX overlay.
        // Wire MoxController::moxStateChanged → SpectrumWidget::setMoxOverlay.
        // From Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter:
        // the flag drives grid pen selection (tx_vgrid_pen red vs rx grey).
        // In 3M-1a we render a 3 px red border tint; full grid recolouring
        // is deferred to 3M-3.
        // Qt::QueuedConnection: MoxController and SpectrumWidget both live on
        // the main thread but a queued connection is used to match the deferred
        // pattern established for the hardwareFlipped connect above.
        if (activeSpectrumWidget()) {
            connect(mox, &MoxController::moxStateChanged,
                    activeSpectrumWidget(), &SpectrumWidget::setMoxOverlay,
                    Qt::QueuedConnection);
        }

        // ── Phase 3M-4 Task 12: SpectrumWidget IMD overlay state wiring ──────
        // From Thetis display.cs:5008 [v2.10.3.13] show condition:
        //   show_imd_measurements = local_mox && _testing_imd
        //                           && _show_imd_measurements && displayduplex;
        // local_mox is already wired above. Wire the other two flags from
        // their authoritative coordinators:
        //   testing_imd            <- TwoToneController::twoToneActiveChanged
        //                              (mirrors Thetis Display.TestingIMD,
        //                              display.cs:296-302 [v2.10.3.13])
        //   show_imd_measurements  <- PureSignal::show2ToneMeasurementsChanged
        //                              (mirrors Thetis Display.ShowIMDMeasurments,
        //                              display.cs:304-311 [v2.10.3.13])
        // displayduplex stays at SpectrumWidget's default (true) — see header.
        if (activeSpectrumWidget()) {
            if (auto* tt = m_radioModel->twoToneController()) {
                connect(tt, &TwoToneController::twoToneActiveChanged,
                        activeSpectrumWidget(), &SpectrumWidget::setTestingIMD);
            }
            // Phase 3M-4 bench-fix: PureSignal coordinator is late-bound
            // during WDSP-init — at MainWindow build time it's typically
            // nullptr, so the connection below never gets made and the
            // IMD overlay show condition stays at m_showIMDMeasurements=
            // false even after the user checks chkShow2ToneMeasurements.
            // Subscribe to RadioModel::pureSignalCoordinatorReady so we
            // re-attempt the connect when the coordinator becomes available.
            // Mirrors the pattern in PureSignalApplet.cpp:118-123 +
            // PsaIndicatorWidget bench-fix.
            auto wireSpectrumToPs = [this](PureSignal* ps) {
                if (!ps || !activeSpectrumWidget()) { return; }
                connect(ps, &PureSignal::show2ToneMeasurementsChanged,
                        activeSpectrumWidget(),
                        &SpectrumWidget::setShowIMDMeasurements,
                        Qt::UniqueConnection);
            };
            wireSpectrumToPs(m_radioModel->pureSignal());
            connect(m_radioModel, &RadioModel::pureSignalCoordinatorReady,
                    this, [wireSpectrumToPs](PureSignal* ps) {
                        wireSpectrumToPs(ps);
                    });
        }

        // ── 3M-1c Phase L.3: VFO TX badge routing ─────────────────────────────
        //
        // MoxController::moxChanged(rx, oldMox, newMox) → VfoDisplayItem
        // setTransmitting on every VfoDisplayItem hosted by the app.  The rx
        // semantic (Thetis console.cs:29677 [v2.10.3.13]) is:
        //   rx==1  → VFO-A (TX comes off VFO-A in 3M-1; default in NereusSDR)
        //   rx==2  → VFO-B (only when RX2 enabled AND VFOBTX — neither
        //                    plumbed in NereusSDR today)
        //
        // Lookup strategy: walk every container's MeterWidget and update
        // every VfoDisplayItem found.  This is coarse but correct for 3M-1
        // (one VFO instance) — when RX2 lands (3F multi-pan), upgrade to
        // per-VfoDisplayItem item-name routing so VFO-B gets rx==2 only.
        //
        // The G.2 routing test (tst_vfo_display_item_tx_badge.cpp) demonstrates
        // the canonical lambda shape that this code mirrors at production scale.
        // TODO [3F]: split routing per-item so RX2's VFO-B instance only
        // updates on rx==2.
        connect(mox, &MoxController::moxChanged, this,
                [this](int rx, bool /*oldMox*/, bool newMox) {
            if (!m_containerManager) { return; }
            // 3M-1: only rx==1 is ever emitted (default RX2/VFOBTX both
            // false in MoxController), so the broadcast fires the same set
            // of items.  Filter on rx==1 to leave the door open for the
            // 3F upgrade without changing the connect site.
            if (rx != 1) { return; }
            for (ContainerWidget* c : m_containerManager->allContainers()) {
                if (!c) { continue; }
                auto* mw = qobject_cast<MeterWidget*>(c->content());
                if (!mw) { continue; }
                for (MeterItem* item : mw->items()) {
                    if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
                        vfo->setTransmitting(newMox);
                    }
                }
            }
        }, Qt::QueuedConnection);
    }

    // --- Phase 3G-9c: Clarity adaptive display tuning ---
    m_clarityController = new ClarityController(this);
    m_radioModel->setClarityController(m_clarityController);

    // Restore enabled state from AppSettings + sync the clarityActive
    // flag on SpectrumWidget so legacy AGC knows to stand down.
    {
        auto& s = AppSettings::instance();
        // Ship default 2026-04-30: Clarity ON for fresh installs. Auto-tuning
        // the noise floor is the better first-launch experience than asking
        // the user to find and toggle the setting themselves.
        bool clarityOn = s.value(QStringLiteral("ClarityEnabled"), QStringLiteral("True"))
                            .toString() == QStringLiteral("True");
        m_clarityController->setEnabled(clarityOn);
        activeSpectrumWidget()->setClarityActive(clarityOn);
    }

    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main).
    // Primary engine only: ClarityController holds one adaptive-display
    // state, so it tracks stream 0 rather than whichever stream last
    // produced a frame. Per-stream Clarity is Phase 3F follow-up work.
    connect(primaryFftEngine(), &FFTEngine::fftReady,
            m_clarityController, [this](int /*rxId*/, const QVector<float>& binsDbm) {
        m_clarityController->feedBins(binsDbm);
    });

    // ── NoiseFloorTracker for Auto AGC-T ────────────────────────────────
    auto* nfTracker = new NoiseFloorTracker;
    m_radioModel->setNoiseFloorTracker(nfTracker);

    connect(primaryFftEngine(), &FFTEngine::fftReady,
            this, [nfTracker](int /*rxId*/, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nfTracker->feed(binsDbm, kFrameIntervalMs);
    });

    // Max Bin detector: feed FFTEngine dBm bins into WdspEngine's NereusSDR-native
    // Max Bin pipeline.  See WdspEngine::setupMaxBinDetector for the algorithm
    // cite and the divergence rationale (WDSP analyzer not wired; FFTEngine
    // uses raw FFTW3 directly; NereusSDR runs the same Thetis algorithm against
    // the dBm bins emitted here).
    //
    // Algorithm from Thetis wdsp/analyzer.c:800-822 [@501e3f5].
    //
    // Primary engine only: setupMaxBinDetector is called with disp=0 (single
    // display channel), so the detector reads stream 0's bins.
    if (auto* eng = (m_radioModel ? m_radioModel->wdspEngine() : nullptr)) {
        connect(primaryFftEngine(), &FFTEngine::fftReady,
                eng,               &WdspEngine::onSpectrumBinsForMaxBin);
    }

    // 2026-05-22 bench fix: MaxBin meter accuracy. The raw FFT bin path
    // above reads ~12-17 dB below what the spectrum visually displays
    // because the spectrum runs the bins through a detector + invEnb
    // window-normalization + avenger time-smoothing pipeline that
    // reconstructs window-spread integrated power. After every spectrum
    // render frame, push the slice's passband peak (from m_renderedPixels,
    // post detector + avenger) into the MaxBin detector so the analog
    // S-meter reads what the operator actually sees on the trace.
    if (activeSpectrumWidget() && m_radioModel) {
        connect(activeSpectrumWidget(), &SpectrumWidget::spectrumFrameRendered,
                this, [this]() {
            auto* eng = m_radioModel ? m_radioModel->wdspEngine() : nullptr;
            if (!eng || !activeSpectrumWidget()) { return; }
            const double dbm = activeSpectrumWidget()->peakDbmInSlicePassband();
            if (dbm > -400.0) {
                eng->setMaxBinDbmFromSpectrum(/*disp=*/0, dbm);
            }
        });
    }

    // Fast-attack triggers — deferred until slice exists
    // From Thetis v2.10.3.13 display.cs:905 — freq change triggers fast attack
    // From Thetis v2.10.3.13 display.cs:880 — mode change triggers fast attack
    // Connected in wireSliceToSpectrum() where activeSlice() is guaranteed non-null
    // From Thetis v2.10.3.13 display.cs:890 — OnAttenuatorDataChanged
    if (m_stepAttController) {
        connect(m_stepAttController, &StepAttenuatorController::attenuationChanged,
                this, [nfTracker](int /*dB*/) {
            nfTracker->triggerFastAttack();
        });
    }

    // Periodic visual update: auto-AGC timer → refresh NF visuals on both widgets
    if (m_radioModel->autoAgcTimer()) {
        connect(m_radioModel->autoAgcTimer(), &QTimer::timeout, this, [this]() {
            SliceModel* s = m_radioModel->activeSlice();
            auto* nft = m_radioModel->noiseFloorTracker();
            if (s && s->autoAgcEnabled() && nft) {
                float nf = nft->noiseFloor();
                double offset = s->autoAgcOffset();
                if (m_vfoWidget) {
                    m_vfoWidget->updateAgcAutoVisuals(true, nf, offset);
                }
                if (m_rxApplet) {
                    m_rxApplet->updateAgcAutoVisuals(true, nf, offset);
                }
            }
        });
    }

    // TX pause: MOX signal → ClarityController
    connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
            m_clarityController, &ClarityController::setTransmitting);

    // Plan 4 D9 (Cluster E): TX filter audio range → spectrum overlay.
    // TransmitModel::filterChanged carries (low, high) audio Hz; SpectrumWidget
    // converts to IQ-space at draw time using m_txMode (set below via slice).
    if (activeSpectrumWidget()) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::filterChanged,
                activeSpectrumWidget(), &SpectrumWidget::setTxFilterRange);

        // Initial sync from current TransmitModel state.
        const auto& txModel = m_radioModel->transmitModel();
        activeSpectrumWidget()->setTxFilterRange(txModel.filterLow(), txModel.filterHigh());
    }

    // Clarity → SpectrumWidget threshold update + clarityActive flag.
    // Issue #230 fix: write the render-active mirror, not the
    // persistent user fields — Clarity is runtime state per Thetis's
    // AGC pattern (display.cs:6584 [v2.10.3.13] uses
    // _RX1waterfallPreviousMinValue, a runtime field separate from
    // waterfall_low_threshold).  The previous setWfLow/HighThreshold
    // calls were silently overwriting the user's saved thresholds via
    // scheduleSettingsSave() on every Clarity tick.
    connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
            activeSpectrumWidget(), [this](float low, float high) {
        activeSpectrumWidget()->setClarityActive(true);
        activeSpectrumWidget()->setClarityWaterfallThresholds(low, high);
    });

    // Clarity → SpectrumWidget NF-aware grid (Task 2.9).
    // NereusSDR-original — no Thetis equivalent.
    // noiseFloorChanged fires after EWMA smoothing but before the deadband
    // gate so the grid tracks the floor at every cadence tick.
    connect(m_clarityController, &ClarityController::noiseFloorChanged,
            activeSpectrumWidget(), &SpectrumWidget::onNoiseFloorChanged);

    // Task 2.10: per-band NF priming — settle detector.
    // NereusSDR-original — no Thetis equivalent.
    //
    // On each noiseFloorChanged tick, keep a 2-second sliding window of NF
    // samples. When variance drops below 1 dB for a sustained window of ≥30
    // samples (≈ 15 s / cadence-0.5s = 30 ticks), save the current floor to
    // the panadapter's per-band NF slot so the next band-switch can snap
    // instantly instead of cold-starting from zero.
    {
        struct NFHistoryEntry { qint64 t; float value; };
        struct SettleState {
            QList<NFHistoryEntry> history;
        };
        auto settle = QSharedPointer<SettleState>::create();

        PanadapterModel* pan0 = m_radioModel->panadapters().isEmpty()
                                ? nullptr
                                : m_radioModel->panadapters().first();
        if (pan0) {
            connect(m_clarityController, &ClarityController::noiseFloorChanged,
                    this, [pan0, settle](float nf) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                settle->history.append({now, nf});

                // Trim to 2-second window.
                const qint64 cutoff = now - 2000;
                while (!settle->history.isEmpty() && settle->history.first().t < cutoff) {
                    settle->history.removeFirst();
                }

                // Compute variance when we have ≥30 samples (~30 cadence ticks).
                if (settle->history.size() >= 30) {
                    float sum = 0.0f;
                    for (const auto& e : std::as_const(settle->history)) { sum += e.value; }
                    const float mean = sum / static_cast<float>(settle->history.size());
                    float sqSum = 0.0f;
                    for (const auto& e : std::as_const(settle->history)) {
                        const float d = e.value - mean;
                        sqSum += d * d;
                    }
                    const float variance = sqSum / static_cast<float>(settle->history.size());

                    if (variance < 1.0f) {
                        // NereusSDR-original — no Thetis equivalent.
                        // NF settled within 1 dB variance over 2s; save for this band.
                        pan0->setBandNFEstimate(pan0->band(), nf);
                    }
                }
            });

            // Task 2.10: band-change → prime ClarityController EWMA with stored NF.
            // NereusSDR-original — no Thetis equivalent.
            //
            // PanadapterModel::bandChanged fires when the pan center crosses a band
            // boundary. snapToFloor() seeds the EWMA (m_smoothedFloor) and emits
            // waterfallThresholdsChanged immediately so the waterfall snaps to the
            // remembered state rather than cold-starting from an uninitialized floor.
            // NaN is ignored by snapToFloor (band with no stored data is a no-op).
            connect(pan0, &PanadapterModel::bandChanged,
                    this, [this, pan0](NereusSDR::Band newBand) {
                // NereusSDR-original — no Thetis equivalent.
                // Prime estimator with last-seen NF for this band to eliminate
                // cold-start visual jump after band change.
                const float storedNF = pan0->bandNFEstimate(newBand);
                m_clarityController->snapToFloor(storedNF);
            });

            // NF fast-attack triggers — From Thetis display.cs:879-905
            // [v2.10.3.13]:
            //   if (rx == 1) FastAttackNoiseFloorRX1 = true;  // band change
            //   if (Math.Abs(oldFreq - newFreq) > 0.5)         // freq jump
            //       FastAttackNoiseFloorRX1 = true;
            // While in fast-attack state SpectrumWidget renders the NF
            // line/box/text in gray to signal the smoothed estimate is
            // still settling.  Auto-clear is internal to the setter (see
            // SpectrumWidget::setNoiseFloorFastAttack — 1000ms timer
            // matching Thetis display.cs:5906 minimum delay).
            if (activeSpectrumWidget()) {
                connect(pan0, &PanadapterModel::bandChanged,
                        this, [this](NereusSDR::Band) {
                    activeSpectrumWidget()->setNoiseFloorFastAttack(true);
                });
            }
        }
    }

    // Slice freq-jump > 0.5 MHz fast-attack trigger — Thetis display.cs:905
    // [v2.10.3.13]: if (Math.Abs(oldFreq - newFreq) > 0.5) FastAttack = true.
    // Smaller jumps (in-band tuning) don't shift the noise floor enough to
    // warrant resetting the smoothed estimate.
    //
    // Stores the last-trigger frequency as a QObject dynamic property on
    // the slice itself — Qt cleans it up when the slice is destroyed, and
    // the same wiring works for slices added later via RadioModel::sliceAdded.
    if (activeSpectrumWidget()) {
        // 500 kHz threshold matches Thetis display.cs:905: > 0.5 MHz.
        constexpr double kFastAttackFreqJumpHz = 500000.0;
        constexpr const char* kLastFreqProp = "nfLastFastAttackFreq";

        auto subscribeSlice = [this](SliceModel* slice) {
            if (!slice) { return; }
            slice->setProperty(kLastFreqProp, slice->frequency());
            connect(slice, &SliceModel::frequencyChanged, this,
                    [this, slice](double freq) {
                const double last =
                    slice->property(kLastFreqProp).toDouble();
                if (std::abs(last - freq) > kFastAttackFreqJumpHz) {
                    activeSpectrumWidget()->setNoiseFloorFastAttack(true);
                }
                slice->setProperty(kLastFreqProp, freq);
            });
        };
        for (SliceModel* slice : m_radioModel->slices()) {
            subscribeSlice(slice);
        }
        connect(m_radioModel, &RadioModel::sliceAdded, this,
                [this, subscribeSlice](int index) {
            subscribeSlice(sliceForAddedIdForTest(m_radioModel, index));
        });
    }

    // Phase 3F Sub-Epic C Task 8: toast on slice-add rejection.
    // RadioModel::addSliceOnPan() emits sliceAddRejected(reason) when the
    // SKU cap blocks a +RX click (e.g. "Hermes Lite 2 supports a maximum
    // of 1 slices"). Surface that for 4 seconds so the operator sees why
    // the click did nothing.
    connect(m_radioModel, &RadioModel::sliceAddRejected, this,
            [this](const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 4000);
    });

    // Phase 3F Sub-Epic I closeout, defect F4.
    //
    // The operator turned the knob to somewhere no DDC can reach. The VFO has
    // already snapped back to the last frequency that bound, so the message
    // has to explain the snap rather than talk about adding a slice, which is
    // what the rejection used to say. 6 s: it names a frequency the operator
    // needs time to read.
    connect(m_radioModel, &RadioModel::sliceRetuneRejected, this,
            [this](int, const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 6000);
    });

    // Phase 3F Sub-Epic I closeout, defect F3.
    //
    // The 1-ADC HERMES class drops every extra receiver the moment PureSignal
    // transmits or diversity engages. That is what Thetis does and it stays,
    // but Thetis says nothing about it either, so on the bench a slice simply
    // stopped producing audio with no explanation. Same surface as the
    // rejection message above; 6 s because it names slice letters the
    // operator has to map back to their flags.
    //
    // This is the notice that produced the 2026-07-30 bench report: it fires
    // on MOX with PureSignal running, which is exactly when the operator most
    // needs the PureSignal indicator and the TX badge it used to cover. It is
    // a toast for that reason and must stay one.
    connect(m_radioModel, &RadioModel::streamsSuspended, this,
            [this](const QVector<int>& streams, const QString& reason) {
        if (streams.isEmpty()) {
            // Everything is back. Take the notice down rather than leaving a
            // stale one on screen for its full timeout: on unkey the streams
            // return in well under six seconds.
            if (m_suspendToast) { m_suspendToast->close(); }
            return;
        }
        m_suspendToast = showToast(reason, ToastSeverity::Warning, 6000);
    });

    // Phase 3F closeout — Sub-Epic E Task 6 consumer wire-up.
    // antennaAutoSwitched(sliceIdx, oldAnt, newAnt) is emitted when an
    // AlexController conflict-policy re-route moves a slice off its old
    // antenna onto a new one. MainWindow constructs an AntennaSwitchToast
    // anchored to the MainWindow bottom-right corner, 8 s auto-dismiss,
    // UNDO button logs (real undo wires when the conflict-detection state
    // machine lands).
    connect(m_radioModel, &RadioModel::antennaAutoSwitched, this,
            [this](int sliceIdx, const QString& oldAnt, const QString& newAnt) {
        const QString msg = QStringLiteral("Slice %1 moved from %2 to %3.")
                                .arg(QChar(QLatin1Char('A' + sliceIdx)),
                                     oldAnt, newAnt);
        auto* toast = new AntennaSwitchToast(msg, this);
        toast->setAttribute(Qt::WA_DeleteOnClose);
        const QRect mwGeom = frameGeometry();
        toast->move(mwGeom.right() - toast->width() - 20,
                     mwGeom.bottom() - toast->height() - 50);
        toast->show();
        connect(toast, &AntennaSwitchToast::undoRequested, this,
                [sliceIdx, oldAnt]() {
            qCInfo(lcContainer) << "AntennaSwitchToast: undo requested for slice"
                                 << sliceIdx << "(would revert to" << oldAnt
                                 << ")  -  real undo wires when conflict-detection lands";
        });
    });

    // Phase 3F closeout — Sub-Epic E Task 7 consumer wire-up.
    // txBoundReRouteRequested(proposedAntenna, existingAntenna) opens a
    // modal TxBoundConfirmDialog with three outcomes (Cancelled /
    // UseExistingAntenna / ConfirmReroute). Today we log the outcome;
    // outcome routing to AlexController lands when the conflict-detection
    // state machine in addSliceOnPan ships in a follow-up.
    connect(m_radioModel, &RadioModel::txBoundReRouteRequested, this,
            [this](const QString& proposed, const QString& existing) {
        if (!m_radioModel) { return; }
        TxBoundConfirmDialog dlg(proposed, existing,
                                  m_radioModel->slices(), this);
        dlg.exec();
        qCInfo(lcContainer) << "TxBoundConfirmDialog: outcome="
                             << int(dlg.outcome())
                             << "(0=Cancelled, 1=UseExistingAntenna, 2=ConfirmReroute)";
    });

    // Phase 3F Sub-Epic C Task 10: TxSliceArbiter state → UI updates.
    // When the TX-bound slice flips (via VfoWidget badge click in T9, or
    // any future programmatic path), update the matching VfoWidget badge
    // and post a 2-second "TX > Slice X" status toast.
    //
    // Sub-Epic D expands this single-VfoWidget update to iterate the full
    // per-pan flag collection (one VfoWidget per slice on multi-pan
    // layouts).  Phase 3F hotfix 2026-05-27 made that fanout live: every
    // VfoWidget tracked in m_vfoWidgetsBySlice (Slice A + any Slice B+
    // flags auto-created on sliceAdded) now gets its TX badge updated
    // to reflect the arbiter's current bound slice.
    if (TxSliceArbiter* arb = m_radioModel->txSliceArbiter()) {
        connect(arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [this](int oldId, int newId) {
            for (auto it = m_vfoWidgetsBySlice.constBegin();
                 it != m_vfoWidgetsBySlice.constEnd(); ++it) {
                VfoWidget* flag = it.value();
                if (flag) {
                    flag->setTxSlice(flag->sliceIndex() == newId);
                }
            }
            // oldId < 0 is the arbiter's initial bind (TxSliceArbiter::
            // syncToSliceList), not an operator handoff: the transmitter
            // did not move, it acquired its first home when the first slice
            // appeared. Announcing "TX > Slice A" on every connect would be
            // noise about something that did not happen.
            if (oldId < 0) { return; }
            showToast(QStringLiteral("TX > Slice %1")
                          .arg(QChar(QLatin1Char('A' + newId))),
                      ToastSeverity::Info, 2000);
        });
    }

    // MOX transition fast-attack trigger — Thetis display.cs:889-892:
    //   if (rx == 1) FastAttackNoiseFloorRX1 = true;
    // Fires on both RX→TX and TX→RX transitions; the buffer-clear pulse on
    // either edge resets the noise-floor settling window.
    if (activeSpectrumWidget()) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
                this, [this](bool) {
            activeSpectrumWidget()->setNoiseFloorFastAttack(true);
        });
    }

    // When Clarity pauses or is disabled, let legacy AGC resume.
    connect(m_clarityController, &ClarityController::pausedChanged,
            activeSpectrumWidget(), [this](bool paused) {
        if (paused) {
            activeSpectrumWidget()->setClarityActive(false);
        }
    });

    // Clarity ↔ overlay panel: badge + Re-tune button (wired after
    // m_overlayPanel creation in buildUI, which runs before this point).
    if (m_overlayPanel) {
        connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
                m_overlayPanel, [this](float, float) {
            m_overlayPanel->setClarityStatus(/*active=*/true, /*paused=*/false);
        });
        connect(m_clarityController, &ClarityController::pausedChanged,
                m_overlayPanel, [this](bool paused) {
            bool enabled = m_clarityController->isEnabled();
            m_overlayPanel->setClarityStatus(enabled, paused);
        });
        connect(m_overlayPanel, &SpectrumOverlayPanel::clarityRetuneRequested,
                m_clarityController, &ClarityController::retuneNow);

        // B8 Task 20: wire Display-flyout orphaned signals to SpectrumWidget.
        // These three signals were emitted but never connected — moving the
        // WF Gain / WF Black Level sliders and the Scheme combo did nothing.
        connect(m_overlayPanel, &SpectrumOverlayPanel::wfColorGainChanged,
                activeSpectrumWidget(), &SpectrumWidget::setWfColorGain);
        connect(m_overlayPanel, &SpectrumOverlayPanel::wfBlackLevelChanged,
                activeSpectrumWidget(), &SpectrumWidget::setWfBlackLevel);
        connect(m_overlayPanel, &SpectrumOverlayPanel::colorSchemeChanged,
                activeSpectrumWidget(), [this](int idx) {
            // colorSchemeChanged carries a raw combo index (int); setWfColorScheme
            // takes the WfColorScheme enum — adapt with a bounds-checked cast.
            const int schemeCount = static_cast<int>(WfColorScheme::Count);
            activeSpectrumWidget()->setWfColorScheme(
                static_cast<WfColorScheme>(qBound(0, idx, schemeCount - 1)));
        });

        // B8 Task 21: wire Cursor Freq toggle to SpectrumWidget visibility guard.
        connect(m_overlayPanel, &SpectrumOverlayPanel::cursorFreqVisibleChanged,
                activeSpectrumWidget(), &SpectrumWidget::setCursorFreqVisible);

        // B8 Task 22: wire Fill Color button to SpectrumWidget::setFillColor.
        connect(m_overlayPanel, &SpectrumOverlayPanel::fillColorChanged,
                activeSpectrumWidget(), &SpectrumWidget::setFillColor);

        // B8 fix-up: wire Fill Alpha slider to SpectrumWidget::setFillAlpha.
        // The slider emitted fillAlphaChanged but had no connect — opacity
        // never reached the renderer.
        connect(m_overlayPanel, &SpectrumOverlayPanel::fillAlphaChanged,
                activeSpectrumWidget(), &SpectrumWidget::setFillAlpha);

        // B8 Task 24: wire "More Display Options →" link to Setup → Display.
        connect(m_overlayPanel, &SpectrumOverlayPanel::openSetupRequested,
                this, [this](const QString& page) {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(page);
            dialog->show();
        });
    }

    // Wire: zoom changes -> auto-replan FFT size to maintain constant
    // bins-per-pixel across zoom levels.  NereusSDR-original (Thetis
    // does not auto-replan on zoom; the user manually picks FFT size).
    //
    // Math: the slider's "FFT size at full DDC bandwidth" baseline
    // implies a target K = baseline / displayWidth bins per pixel.
    // To maintain K as bwHz narrows (zoom in), the FFT size must scale
    // inversely with bwHz:
    //   targetSize = baseline * sampleRate / bwHz
    //
    // Cap at kAutoZoomMaxFftSize = 65536.  Set well below kMaxFftSize
    // (262144) to bound the buffer-fill pause on every replan: at
    // 768 kHz DDC, 65536 fills in 85 ms (barely perceptible).  262144
    // would take 340 ms (jarring) and create a multi-frame avenger
    // ghost in the waterfall as the smoothed state crosses fftSize
    // resolutions.  Users who want larger FFTs explicitly opt in via
    // the slider (one-time pause they chose); auto-zoom won't push
    // above the cap automatically.
    //
    // Floor at the slider baseline (we never replan BELOW the user's
    // chosen value).
    //
    // Hysteresis: only replan when computed/current is outside
    // [0.66, 1.5].  Avoids replan thrash on smooth zoom drag.
    //
    // Phase 3F Sub-Epic I Task 8: the primary engine, because this lambda is
    // wired to pan 0's SpectrumWidget (the signal is per-pan). Per-pan
    // auto-zoom on secondary pans is Phase 3F follow-up work.
    constexpr int kAutoZoomMaxFftSize = 65536;
    connect(activeSpectrumWidget(), &SpectrumWidget::bandwidthChangeRequested,
            this, [this, kAutoZoomMaxFftSize](double bwHz) {
        FFTEngine* engine = primaryFftEngine();
        if (!engine || !activeSpectrumWidget()) { return; }
        const double sampleRate = activeSpectrumWidget()->sampleRate();
        if (sampleRate <= 0.0 || bwHz <= 0.0) { return; }

        const int baseline = engine->fftSizeBaseline();

        // Hz/bin override (Option 3 from the 2026-05-08 design).  When the
        // user has set a non-zero target Hz/bin in
        // Setup → Display → Spectrum Defaults, the auto-zoom formula
        // becomes zoom-INDEPENDENT:
        //   targetSize = sampleRate / hzPerBinTarget
        // The FFT delivers the requested resolution at any zoom — useful
        // for hunting narrow features (CW, digital).  Floor at baseline
        // still applies, so the FFT slider remains a minimum-FFT-size
        // knob.  When hzPerBinTarget == 0 we use the original
        // bins-in-window default (constant K = baseline).
        const double hzPerBinTarget = engine->hzPerBinTarget();
        double desired;
        if (hzPerBinTarget > 0.0) {
            desired = sampleRate / hzPerBinTarget;
        } else {
            const double scale = sampleRate / bwHz;        // 1.0 at full bw
            desired = static_cast<double>(baseline) * scale;
        }

        // Round up to next power of 2.
        int targetSize = 1024;
        while (targetSize < desired && targetSize < kAutoZoomMaxFftSize) {
            targetSize *= 2;
        }
        // Floor at baseline (slider's choice always honoured), then cap at
        // auto-zoom max.  When baseline > cap (user explicitly picked a
        // larger size via the slider), baseline wins and auto-zoom is a
        // no-op for that range.
        targetSize = (std::max)(targetSize, baseline);
        targetSize = (std::min)(targetSize, (std::max)(baseline, kAutoZoomMaxFftSize));

        // Hysteresis: only replan if outside [current * 2/3, current * 3/2].
        const int currentSize = engine->fftSize();
        if (currentSize > 0) {
            const double ratio = static_cast<double>(targetSize)
                                 / static_cast<double>(currentSize);
            if (ratio > 0.66 && ratio < 1.5) {
                return;  // small enough change to ignore
            }
        }

        engine->setFftSize(targetSize);
    });

    m_fftThread->start();

    // --- Meter Poller (Phase 3G-2) ---
    // Poller was created earlier so the meterReadyForPolling signal
    // could catch container restore + populateDefaultMeter emits.
    // m_meterWidget is registered automatically via that signal —
    // the call here is a defensive belt only and dedupes inside
    // MeterPoller::addTarget.
    if (m_meterWidget) {
        m_meterPoller->addTarget(m_meterWidget);
    }

    // Wire RxChannel to poller when WDSP finishes initializing.
    // RadioModel's initializedChanged handler creates the RxChannel, but
    // it was registered AFTER this connection (RadioModel registers during
    // onConnectionStateChanged, not buildUI). Qt fires in registration order,
    // so we defer by one event loop pass to ensure RxChannel exists.
    connect(m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
            this, [this](bool ok) {
        if (!ok) { return; }
        QTimer::singleShot(0, this, [this]() {
            // Phase 3F Sub-Epic J Task 11: RadioModel::rxChannelForSlice()
            // replaces the direct wdspEngine()->rxChannel() reach. Still
            // channel 0 here on purpose -- this is the boot-time seed, before
            // any slice but A exists, and the activeSliceChanged handler
            // below immediately supersedes it once other slices are added.
            RxChannel* rxCh = m_radioModel->rxChannelForSlice(0);
            if (rxCh) {
                m_meterPoller->setRxChannel(rxCh);
                m_meterPoller->start();
                qCDebug(lcMeter) << "MeterPoller started on RxChannel 0";
            } else {
                qCWarning(lcMeter) << "MeterPoller: RxChannel 0 still null after WDSP init";
            }

            // H.2 (Phase 3M-1a): wire TxChannel to MeterPoller for TX meters.
            // TxChannel is valid after WDSP initialization via createTxChannel().
            // Guard: txChannel() is null before initialization; null guard in
            // pollTxMeters() handles the case where it isn't set yet.
            if (TxChannel* txCh = m_radioModel->txChannel()) {
                m_meterPoller->setTxChannel(txCh);
                qCDebug(lcMeter) << "MeterPoller: TxChannel wired for TX meters";
            }
        });
    });

    // Phase 3F Sub-Epic J Task 4: the container S-meter is attached to no
    // flag, so it must show the active slice, not always slice A. The
    // WDSP-init binding above is only the seed for the first slice --
    // MeterPoller::pollSMeter() (which drives the analog SMeterWidget
    // installed as AppletPanelWidget's fixed header, i.e. the widget this
    // wiring targets) reads m_rxChannel exclusively and that pointer never
    // moved after the seed, so the container meter showed RxChannel 0
    // whatever the operator was working. The per-flag mini S-meters do not
    // have this bug: they already resolve their own channel per slice via
    // MeterPoller::pollSliceSMeters() (wdspEngine()->rxChannel(sliceId)), so
    // they are untouched here.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this, [this](int) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }
        // Phase 3F Sub-Epic J Task 11: RadioModel::rxChannelForSlice()
        // replaces the direct wdspEngine()->rxChannel() reach.
        RxChannel* rxCh = m_radioModel->rxChannelForSlice(slice->sliceIndex());
        if (rxCh) { m_meterPoller->setRxChannel(rxCh); }
    });

    // H.2 (Phase 3M-1a): wire MoxController::moxStateChanged → MeterPoller::setInTx.
    // Switches the poll set between RX meters (TX off) and TX meters (TX on).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter dispatch.
    // Qt::QueuedConnection: ensures the flip happens at the start of the next
    // event loop tick rather than mid-poll, matching Thetis's timer dispatch.
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::moxStateChanged,
                m_meterPoller, &MeterPoller::setInTx,
                Qt::QueuedConnection);

        // ── Phase 3M-1b K.2: MOX rejection → toast ──────────────────────────
        // moxRejected fires when BandPlanGuard::checkMoxAllowed() rejects a
        // setMox(true) request (wrong mode, out-of-band freq, cross-band TX).
        // Presented for 3 seconds, matching the bandClickIgnored pattern.
        // The toast is transient: it clears automatically and does not affect
        // bottom-bar layout or persistence.
        connect(mox, &MoxController::moxRejected,
                this, [this](const QString& reason) {
            showToast(reason, ToastSeverity::Warning, 3000);
        });
    }

    // ── Phase 3M-0 Task 17: safety controller → status-bar wiring ────────────
    //
    // Wire PA Fwd/Rev/SWR telemetry into MeterPoller's cache so PA power
    // meter items (MeterPoller::setRadioStatus) receive live data as
    // RadioStatus::powerChanged fires. Called here (after poller creation)
    // so the connection outlives the poller's lifetime.
    m_meterPoller->setRadioStatus(&m_radioModel->radioStatus());

    // Ganymede PA-trip badge: RadioModel::paTrippedChanged → setPaTripped.
    // setPaTripped() was added in Task 14 and updates m_paStatusBadge text
    // and colour atomically.
    connect(m_radioModel, &RadioModel::paTrippedChanged,
            this, &MainWindow::setPaTripped);

    // TX Inhibit pill: TxInhibitMonitor::txInhibitedChanged → setTxInhibited.
    // setTxInhibited() was added in Task 14 and toggles m_txInhibitLabel
    // visibility. The Source parameter is ignored by the UI slot (the pill
    // is binary: visible or hidden).
    connect(&m_radioModel->txInhibit(),
            &safety::TxInhibitMonitor::txInhibitedChanged,
            this, [this](bool inhibited, safety::TxInhibitMonitor::Source /*source*/) {
        setTxInhibited(inhibited);
    });
}

void MainWindow::rebuildEditContainerSubmenu()
{
    if (!m_editContainerMenu) { return; }
    m_editContainerMenu->clear();

    if (!m_containerManager) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    if (all.isEmpty()) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    // Alphabetical by title so the menu order is predictable even
    // when containers are created in different orders.
    QList<ContainerWidget*> sorted = all;
    std::sort(sorted.begin(), sorted.end(),
              [](ContainerWidget* a, ContainerWidget* b) {
        const QString na = a->notes().isEmpty()
            ? a->id().left(8) : a->notes();
        const QString nb = b->notes().isEmpty()
            ? b->id().left(8) : b->notes();
        return na.localeAwareCompare(nb) < 0;
    });

    for (ContainerWidget* c : sorted) {
        const QString label = c->notes().isEmpty()
            ? (QStringLiteral("(unnamed) ") + c->id().left(8))
            : c->notes();
        QAction* act = m_editContainerMenu->addAction(label);
        const QString id = c->id();
        connect(act, &QAction::triggered, this, [this, id]() {
            if (!m_containerManager) { return; }
            ContainerWidget* target = m_containerManager->container(id);
            if (!target) { return; }
            ContainerSettingsDialog dialog(target, this, m_containerManager);
            dialog.exec();
        });
    }
}

void MainWindow::resetDefaultLayout()
{
    if (!m_containerManager) { return; }

    // Destroy every non-panel container. Collect IDs first because
    // destroyContainer mutates the underlying map.
    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    ContainerWidget* panel = m_containerManager->panelContainer();
    QStringList toRemove;
    for (ContainerWidget* c : all) {
        if (c == panel) { continue; }
        toRemove.append(c->id());
    }
    for (const QString& id : toRemove) {
        m_containerManager->destroyContainer(id);
    }

    // Also wipe the panel container's MeterWidget items and rebuild
    // from factories. Before this, a persisted item payload (e.g. a
    // bar-style S-Meter saved by an earlier build) would survive
    // Reset because only the non-panel containers were destroyed —
    // Container #0's MeterWidget was left untouched, so the stale
    // items reloaded every launch and Reset felt like a no-op for
    // the main meter column.
    if (m_meterWidget) {
        m_meterWidget->clearItems();

        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    rebuildEditContainerSubmenu();
    qCInfo(lcContainer) << "Reset default layout: removed"
                         << toRemove.size() << "non-panel containers"
                         << "and rebuilt panel meter defaults";
}

void MainWindow::createDefaultContainers()
{
    // Container #0: panel-docked right side (AetherSDR pattern).
    // Placeholder content in 3G-1, replaced by AppletPanel in 3G-AP.
    ContainerWidget* c0 = m_containerManager->createContainer(1, DockMode::PanelDocked);
    c0->setNotes(QStringLiteral("Main Panel"));
    c0->setNoControls(false);

    qCDebug(lcContainer) << "Created default Container #0 (panel-docked):" << c0->id();
}

void MainWindow::populateDefaultMeter()
{
    ContainerWidget* c0 = m_containerManager->panelContainer();
    if (!c0) {
        qCWarning(lcContainer) << "No panel container for meter widget";
        return;
    }

    // Guard: don't repopulate if real content (AppletPanelWidget) already exists.
    // The container constructor creates a placeholder QLabel("Container") which
    // we need to replace. Check if content is already an AppletPanelWidget.
    if (qobject_cast<AppletPanelWidget*>(c0->content()) != nullptr) {
        return;
    }

    // ContainerManager::restoreState may have set a MeterWidget with
    // user-saved items as the panel container's content. Capture that
    // payload before we overwrite c0's content. We can't reuse the
    // pointer directly because ContainerWidget::setContent() calls
    // deleteLater on the previous m_content; round-tripping through
    // serialize/deserialize transfers the items into a fresh
    // m_meterWidget that becomes the AppletPanelWidget header.
    QString restoredItems;
    if (auto* existingMeter = qobject_cast<MeterWidget*>(c0->content())) {
        if (!existingMeter->items().isEmpty()) {
            restoredItems = existingMeter->serializeItems();
        }
    }

    m_meterWidget = new MeterWidget();

    if (!restoredItems.isEmpty()) {
        m_meterWidget->deserializeItems(restoredItems);
        // Rebuild the runtime stack metadata from geometry so bar
        // rows restored from a saved container participate in the
        // reflow-on-resize path again. No-op for panels that don't
        // contain a stack.
        m_meterWidget->inferStackFromGeometry();
        qCDebug(lcContainer) << "Restored" << m_meterWidget->items().size()
                             << "panel meter items from saved state";
    } else {
        // S-Meter: top 45% — arc needle bound to SignalAvg
        // From Thetis MeterManager.cs: ANAN needle uses AVG_SIGNAL_STRENGTH
        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        // Power/SWR: middle 40% — stacked bars (stub TX bindings)
        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        // ALC: bottom 15% — compact single-line bar (stub TX binding)
        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    // Build an AppletPanelWidget: SMeterWidget header + scrollable applets.
    // Task 40 (Phase 3P-II): AppletPanelWidget constructor now installs the
    // analog SMeterWidget as the fixed header automatically.  The old
    // setHeaderWidget(m_meterWidget, ...) call is removed; the composite
    // MeterWidget (m_meterWidget) remains the Container #0 content for the
    // traditional GroupBox-based meters and is not affected.
    m_appletPanel = new AppletPanelWidget();
    auto* panel = m_appletPanel;

    // Task 41 (Phase 3P-II): wire the SMeterWidget (installed by the
    // AppletPanelWidget constructor) into MeterPoller.  WdspEngine is
    // needed for getRxaSignalPeak() and getMaxBinDbm() (MaxBin mode).
    if (m_meterPoller) {
        if (SMeterWidget* sm = m_appletPanel->smeterWidget()) {
            m_meterPoller->setSMeter(sm);
        }
        m_meterPoller->setWdspEngine(m_radioModel->wdspEngine());

        // RX meter cal offset source (Thetis-faithful port).
        // RadioModel::rxMeterOffsetDb() returns RXPreampOffset(1) +
        // RXCalibrationOffset(1) per Thetis console.cs:21040 [v2.10.3.13].
        // The callable captures m_radioModel by raw pointer (lives for
        // the lifetime of MainWindow); pollSMeter / poll invoke it
        // once per tick.  Without this wire-up the WDSP S-meter readings
        // sit in raw ADC dBFS instead of at-antenna dBm.
        m_meterPoller->setRxOffsetSource([rm = m_radioModel]() -> double {
            return rm ? rm->rxMeterOffsetDb() : 0.0;
        });
    }

    // 2026-05-22 spectrum-calibration fix (Fix 1 from the S-meter / spectrum
    // alignment research). FFTEngine bins ship in raw dBFS (window-coherent
    // gain compensated against the digital I/Q full-scale reference). The
    // S-meter path adds the same RXPreampOffset + RXCalibrationOffset chain
    // Thetis applies at console.cs:21040 to land at antenna-referenced dBm,
    // but the spectrum path never got that calibration step. Result: the
    // S-meter and the spectrum trace lived in different reference frames
    // (38 dB gap on the bench at preamp Off; 18 dB at preamp On). Forward
    // the SAME rxMeterOffsetDb value to SpectrumWidget::setDbmCalOffset so
    // both views share the antenna reference. Per the calibration research,
    // carriers will then agree to ~1 dB between meter and spectrum. Noise
    // floor will still differ by 10*log10(NBP_BW/bin_BW) which is physics
    // (S-meter is passband-integrated; spectrum is per-bin) and matches
    // Thetis behavior.
    if (activeSpectrumWidget() && m_radioModel) {
        auto pushSpectrumCal = [this](double db) {
            if (activeSpectrumWidget()) {
                activeSpectrumWidget()->setDbmCalOffset(static_cast<float>(db));
            }
        };
        connect(m_radioModel, &RadioModel::rxMeterOffsetChanged,
                this, pushSpectrumCal);
        // Initial push so the cal lands at startup before any controller
        // change. setStepAttController already calls the recompute lambda
        // once on attach, but that may have run before this connect was
        // wired -- push the current value here defensively.
        pushSpectrumCal(m_radioModel->rxMeterOffsetDb());
    }

    // Refresh MaxBin's CTUN slice offset whenever the DDC center moves.
    // The frequencyChanged lambda in wireSliceToSpectrum pushes the
    // offset on slice retune, but a spectrum pan moves the DDC NCO
    // without moving the slice -- without this hook, MaxBin's scan
    // window stays at the OLD DDC-relative bin range until the next
    // slice retune (or CTUN toggle).  Observable as "MaxBin meter
    // drifts off the carrier when I pan the panadapter."
    if (activeSpectrumWidget()) {
        connect(activeSpectrumWidget(), &SpectrumWidget::ddcCenterFrequencyChanged,
                this, [this](double ddcCenter) {
            if (!m_radioModel) { return; }
            auto* eng = m_radioModel->wdspEngine();
            if (!eng) { return; }
            SliceModel* slice = m_radioModel->activeSlice();
            if (!slice) { return; }
            eng->setMaxBinSliceOffsetHz(/*disp=*/0,
                                        slice->frequency() - ddcCenter);
        });
    }

    // Task 43 (Phase 3P-II): PGXL-aware power scale + TX meter feed.
    //
    // Four permanent connects (RadioModel persists across radio connects):
    //
    // 1. ampMetersChanged: when PGXL is OPERATE, forward the amp's
    //    forward-power/SWR readings to the SMeterWidget TX display.
    //    RadioModel::ampMetersChanged is fired by PgxlConnection on each
    //    statusUpdated containing "peakfwd" and "swr" keys.
    //
    // 2. RadioStatus::powerChanged: when PGXL is absent or STANDBY, use the
    //    radio's own PA telemetry (barefoot or Aurora) for the TX display.
    //    fwd is forward power in watts; the third argument (swr) is used.
    //
    // 3. amplifierChanged: snap the power scale to 2 kW on PGXL connect.
    //
    // 4. ampStateChanged: re-evaluate scale whenever OPERATE/STANDBY toggles.
    //    When returning to STANDBY (amp present but not OPERATE), the scale
    //    reverts to barefoot by passing hasAmplifier()=false to setPowerScale.
    if (SMeterWidget* sm = m_appletPanel->smeterWidget()) {
        // Connect 1: PGXL amp meters (OPERATE path).
        connect(m_radioModel, &RadioModel::ampMetersChanged, this,
                [this, sm](float fwd, float swr) {
            if (m_radioModel->hasAmplifier() && m_radioModel->ampOperate()) {
                sm->setTxMeters(fwd, swr);
            }
        });

        // Connect 2: radio barefoot/Aurora TX meters (STANDBY or no amp).
        // RadioStatus::powerChanged carries (fwdWatts, revWatts, swr); we
        // take fwdWatts (arg 1) and swr (arg 3) to match setTxMeters() signature.
        //
        // 2026-05-25 KG4VCF bench fix: gate on isAnyExternalAmpInOperate
        // (cross-vendor) instead of just PGXL state.  RadioStatus::powerChanged
        // fires much faster than RF-Kit's 1 Hz REST poll, so when RF-Kit
        // (without PGXL) is in OPERATE, the radio's barefoot reading was
        // overwriting the RF-Kit amp reading at ~20 Hz.  Now barefoot only
        // feeds when no external amp is amplifying.
        connect(&m_radioModel->radioStatus(), &RadioStatus::powerChanged,
                this, [this, sm](double fwd, double /*rev*/, double swr) {
            if (!m_radioModel->isAnyExternalAmpInOperate()) {
                sm->setTxMeters(static_cast<float>(fwd), static_cast<float>(swr));
            }
        });

        // Connect 3: PGXL connect event snaps scale to 2 kW.
        connect(m_radioModel, &RadioModel::amplifierChanged, this,
                [sm](bool present) {
            sm->setPowerScale(/*maxWatts=*/0, present);
        });

        // Connect 4: OPERATE/STANDBY state change re-evaluates scale.
        // When STANDBY: hasAmplifier()=true but we want barefoot scale,
        // so pass false (treat as absent) until OPERATE resumes.
        //
        // 2026-05-25 KG4VCF bench fix: use the cross-vendor predicate so
        // PGXL going STANDBY does not flip the SMeterWidget back to
        // barefoot scale if RF-Kit is still amplifying (and vice versa).
        connect(m_radioModel, &RadioModel::ampStateChanged, this,
                [this, sm]() {
            sm->setPowerScale(/*maxWatts=*/0,
                              m_radioModel->isAnyExternalAmpInOperate());
        });

        // Connect 5: 2026-05-20 bench fix -- SMeterWidget::setTransmitting
        // was implemented but never wired. m_transmitting stayed false so
        // updateNeedleTarget() always fell through to the RX dBm path,
        // even when ampMetersChanged was feeding watts via setTxMeters.
        // The needle therefore showed an RX S-meter reading during TX
        // even though PGXL was clearly delivering power. Wire MoxController
        // so the needle switches to the TX-power scale on key, returns to
        // RX scale on unkey. moxStateChanged fires at end of walk so the
        // switch lines up with carrier-on-air.
        if (MoxController* mox = m_radioModel->moxController()) {
            connect(mox, &MoxController::moxStateChanged,
                    sm, &SMeterWidget::setTransmitting);
        }
    }

    // Phase 3P-III review fix C1: wire the production SMeterWidget to the
    // cross-vendor RF-Kit aggregate signals.
    //
    // The displayed SMeterWidget is constructed via the QWidget-only ctor in
    // AppletPanelWidget, so the SMeterWidget(RadioModel*, QWidget*) overload +
    // connectToRadioModel() added in Task 13 are dead code in production.
    // externalAmpOperateChanged and externalAmpFwdSwrUpdated fire correctly
    // from RadioModel but the displayed widget never subscribed, leaving the
    // 2 kW scale switch silent on RF-Kit OPERATE and the TX needle frozen.
    //
    // Wire here, immediately after the analogous PGXL SMeterWidget block,
    // so all SMeterWidget signal connections live in one place. PGXL paths
    // (Connect 1..5 above) remain unchanged; these two add RF-Kit on top.
    if (SMeterWidget* sm = m_appletPanel->smeterWidget()) {
        // RF-Kit Connect A: snap 2 kW scale on RF-Kit OPERATE.
        // externalAmpOperateChanged is now transition-gated (fix I2) so this
        // fires only when the amp enters or leaves OPERATE, not every poll.
        //
        // 2026-05-25 KG4VCF bench fix: use the cross-vendor predicate so
        // RF-Kit going STANDBY does not flip back to barefoot scale if
        // PGXL is still amplifying.
        connect(m_radioModel, &RadioModel::externalAmpOperateChanged, this,
                [this, sm](bool /*inOp*/) {
            sm->setPowerScale(/*maxWatts=*/0,
                              m_radioModel->isAnyExternalAmpInOperate());
        });

        // RF-Kit Connect B: feed RF-Kit forward power + SWR to the TX needle.
        // externalAmpFwdSwrUpdated carries watts (int) and SWR (float);
        // setTxMeters takes (float fwdPower, float swr).
        //
        // 2026-05-25 KG4VCF bench fix: gate so a STANDBY amp doesn't push
        // 0 W into the needle while the radio's barefoot reading is also
        // wanting the meter.  Only fire when the amp is actually amplifying.
        //
        // The gate is RF-Kit's own OPERATE state, not the cross-vendor
        // isAnyExternalAmpInOperate(): this signal carries RF-Kit telemetry
        // exclusively, so the cross-vendor form let a PGXL in OPERATE pass
        // RF-Kit /power polls through from an RF2K-S sitting in STANDBY,
        // clobbering the live PGXL reading with RF-Kit's 0 W once per poll.
        // Codex review, PR #291.
        connect(m_radioModel, &RadioModel::externalAmpFwdSwrUpdated, this,
                [this, sm](int fwd, float swr) {
            if (m_radioModel->isRfKitInOperate()) {
                sm->setTxMeters(static_cast<float>(fwd), swr);
            }
        });
    }

    // Connect 5: Phase 3P-II Phase 4 Task 97 -- PGXL power cap soft-alert.
    // Fires a 5-second status-bar toast when peak forward power exceeds the
    // cap configured in Setup -> Peripherals -> PGXL Advanced -> Hardware.
    // De-bounced: one toast per exceedance event (re-arms below cap).
    connect(m_radioModel, &RadioModel::ampMetersChanged,
            this, &MainWindow::onAmpMetersForPowerCap);

    // Phase 3P-II review fix C2: surface TX interlock decisions to the
    // operator via 5-second status-bar toasts.  Without these connections
    // Block mode silently gates TX with no operator feedback.
    if (TxInterlockPolicy* policy = m_radioModel->txInterlockPolicy()) {
        connect(policy, &TxInterlockPolicy::warned,
                this, &MainWindow::onTxInterlockWarning);
        connect(policy, &TxInterlockPolicy::denied,
                this, &MainWindow::onTxInterlockDenial);
    }

    // RxApplet — Tier 1 wired to SliceModel (slice attached in wireSliceToSpectrum)
    m_rxApplet = new RxApplet(nullptr, m_radioModel, nullptr);
    panel->addApplet(m_rxApplet);

    // Phase 3P-I-a T16 — push board caps into RxApplet so ANT buttons
    // hide on HL2/Atlas. Matches the VFO Flag wiring below (T15).
    // B3: also push SKU profile so AntennaPopupBuilder knows rxOnlyLabels.
    m_rxApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    m_rxApplet->setHpsdrSku(m_radioModel->hardwareProfile().model);
    connect(m_radioModel, &RadioModel::currentRadioChanged, m_rxApplet,
            [this]() {
        m_rxApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
        m_rxApplet->setHpsdrSku(m_radioModel->hardwareProfile().model);
    });

    // ── Phase 3F (Bug 3): per-slice RX applet + active-slice switching ──────
    //
    // Clicking a slice tab in the RX applet, or clicking a VfoWidget flag,
    // routes to RadioModel::setActiveSlice. RadioModel::activeSliceChanged
    // then rebinds the RX applet to the new active slice, refreshes the tab
    // row highlight, and updates the static badge. The slice tab row is also
    // refreshed whenever the slice list changes (add/remove). Workflow ported
    // from AetherSDR (RxApplet::sliceActivationRequested ->
    // MainWindow::setActiveSlice at MainWindow.cpp:3277, and the
    // setActiveSliceInternal rebind at MainWindow.cpp:12132 [@6a142807]).
    //
    // setActiveSliceById for the same reason the flag path uses it:
    // updateSliceButtons keys each tab's button-group id to the slice's own
    // sliceIndex() rather than its list position, so the id has to be
    // converted rather than indexed with.
    connect(m_rxApplet, &RxApplet::sliceActivationRequested, this,
            [this](int sliceId) {
        if (m_radioModel) { m_radioModel->setActiveSliceById(sliceId); }
    });

    auto refreshSliceTabs = [this]() {
        if (m_rxApplet && m_radioModel) {
            m_rxApplet->updateSliceButtons(
                m_radioModel->slices(),
                m_radioModel->activeSlice()
                    ? m_radioModel->activeSlice()->sliceIndex()
                    : -1);
        }
    };

    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int sliceIndex) {
        if (!m_radioModel) { return; }
        SliceModel* active = m_radioModel->activeSlice();
        // Rebind the RX applet to the new active slice + refresh its badge.
        if (m_rxApplet) {
            m_rxApplet->setSlice(active);
            if (active) { m_rxApplet->setSliceIndex(active->sliceIndex()); }
        }
        // Refresh the tab row highlight (uses sliceIndex param for the
        // checked state even if activeSlice() is briefly stale).
        if (m_rxApplet) {
            m_rxApplet->updateSliceButtons(m_radioModel->slices(), sliceIndex);
        }
    });

    // Keep the tab row in sync with the slice population.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [refreshSliceTabs](int) { refreshSliceTabs(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [refreshSliceTabs](int) { refreshSliceTabs(); });

    // TxApplet — NYI shell (Phase 3I-1)
    // 3M-3a-ii Batch 6: cache pointer in m_txApplet so SetupDialog
    // instances can wire CfcSetupPage's [Configure CFC bands…] button
    // through to TxApplet::requestOpenCfcDialog.
    auto* txApplet = new TxApplet(m_radioModel, nullptr);
    m_txApplet = txApplet;
    panel->addApplet(txApplet);

    // ── 3M-1c Phase L: hand TxApplet the controllers it needs ──────────────
    //
    // L.1 — MicProfileManager (J.1 setter): drives the TX Profile combo
    // population + active-profile mirror + "Default" seed surfacing.  The
    // pointer is obtained from RadioModel (constructed in the ctor; per-MAC
    // scope is set inside connectToRadio).  Pre-connect, the manager is
    // unscoped and the combo simply stays at the placeholder "Default" item
    // (rebuildProfileCombo() no-ops when no manager is set).
    //
    // L.2 — TwoToneController (J.2 setter): drives the 2-TONE button toggle
    // round-trip.  The controller's setActive(true) refuses with a
    // qCWarning when m_powerOn is false, so pre-connect button presses are
    // safely rejected.
    //
    // L (J.4) — txProfileMenuRequested signal: a right-click on the profile
    // combo opens SetupDialog at "TX Profile".  Lambda-construct a fresh
    // SetupDialog each time (matches the 7 other "open setup" sites in
    // MainWindow.cpp at lines 1283 / 2824 / 2834 / 2846 / 3029 / 3428).
    if (m_radioModel) {
        txApplet->setMicProfileManager(m_radioModel->micProfileManager());
        txApplet->setTwoToneController(m_radioModel->twoToneController());
    }
    connect(txApplet, &TxApplet::txProfileMenuRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("TX Profile"));
        dialog->show();
    });

    // ── Phase 3M-4 Task 13: PS-A right-click → open PsForm ─────────────────
    // Mirrors Thetis chkFWCATUBypass_MouseDown (console.cs:46149-46152
    // [v2.10.3.13]).  PsForm is the same singleton dialog opened from the
    // Tools / DSP menu and from PureSignalApplet right-click handlers.
    connect(txApplet, &TxApplet::openPureSignalDialogRequested,
            this, &MainWindow::openPureSignalDialog);

    // Phase 3M-4 Task 13 — capability-gated PS-A visibility.  Push initial
    // caps + keep them in sync on RadioModel::currentRadioChanged.  TxApplet
    // hides m_psaBtn when caps.hasPureSignal == false (HL2 / Atlas).
    txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    connect(m_radioModel, &RadioModel::currentRadioChanged, txApplet,
            [this, txApplet]() {
        txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    });

    // Phase 3M-4 Task 13 — late-bound coordinator handoff.  TxApplet
    // already self-subscribes inside its ctor (see TxApplet.cpp wireControls
    // PS-A block), so no explicit connect needed here.  Still: push the
    // current coordinator at startup in case it's already live (test path).
    if (PureSignal* ps = m_radioModel->pureSignal()) {
        txApplet->setPureSignal(ps);
    }

    // 3M-1a H.1-H.4 fixup: wire panadapter band changes to TxApplet so
    // the per-band Tune Power slider tracks the active band.
    // Without this, m_currentBand stays at Band::Band20m permanently.
    if (!m_radioModel->panadapters().isEmpty()) {
        PanadapterModel* pan0 = m_radioModel->panadapters().first();
        connect(pan0, &PanadapterModel::bandChanged,
                txApplet, &TxApplet::setCurrentBand);
        // Push the initial band immediately so the slider shows the right value.
        txApplet->setCurrentBand(pan0->band());
    }

    // 3M-1a (2026-04-27): also track every SLICE'S frequency.  The
    // panadapter band only changes when its center crosses a band
    // boundary, but the user's slice can sit on a different band entirely
    // (e.g. the slice loaded at 7.241 MHz while the panadapter center is
    // still on 14 MHz from a prior session). Without this wire,
    // TxApplet's m_currentBand lags the slice → the TUN-power slider
    // writes to the wrong band's stored value (or no-ops on the
    // m_currentBand-default band — bench-confirmed 2026-04-27 with the
    // slider only working on 20m even after retuning to 40m).
    //
    // Pattern matches AntennaAlexAlex2Tab.cpp:408-425 — subscribe to
    // every current slice AND every future-added slice (slices are
    // created by addSlice() AFTER MainWindow construction, so a single-
    // shot activeSlice() check at construction returns null and never
    // wires up).
    {
        auto subscribeToSlice = [txApplet](SliceModel* slice) {
            if (!slice) { return; }
            connect(slice, &SliceModel::frequencyChanged,
                    txApplet, [txApplet](double freq) {
                        txApplet->setCurrentBand(bandFromFrequency(freq));
                    });
            // Push the slice's current band immediately — overrides the
            // panadapter initial when the slice is on a different band.
            txApplet->setCurrentBand(bandFromFrequency(slice->frequency()));
        };
        for (SliceModel* slice : m_radioModel->slices()) {
            subscribeToSlice(slice);
        }
        connect(m_radioModel, &RadioModel::sliceAdded, txApplet,
                [this, subscribeToSlice](int index) {
                    subscribeToSlice(
                        sliceForAddedIdForTest(m_radioModel, index));
                });
    }

    // PhoneCwApplet — Phone + CW pages, NYI
    m_phoneCwApplet = new PhoneCwApplet(m_radioModel, nullptr);
    panel->addApplet(m_phoneCwApplet);

    // RadeApplet — Phase 3R L2.  Sits alongside PhoneCwApplet but is
    // visible only when the active slice's mode is DSPMode::RADE_U or
    // DSPMode::RADE_L.  The initial mode is set in the dspModeChanged
    // lambda below; for the default startup mode (USB) the applet
    // starts hidden.
    m_radeApplet = new RadeApplet(m_radioModel, nullptr);
    panel->addApplet(m_radeApplet);
    m_radeApplet->setVisible(false);

    // Ghost applets — hidden per docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 6.
    // These applets are entirely placeholder-only today (no wired controls).
    // Showing them is misleading — users click e.g. "Equalizer" and nothing happens.
    // Uncomment each when its feature phase ships (one-line re-enable).
    //
    // m_eqApplet = new EqApplet(m_radioModel, nullptr);           // TODO 3I-3: TX/RX EQ wiring
    // panel->addApplet(m_eqApplet);

    // VaxApplet — per-VAX-channel gain + mute + level meters
    // (Phase 3O Sub-Phase 9 Task 9.2b).
    m_vaxApplet = new VaxApplet(m_radioModel,
                                m_radioModel->audioEngine(), nullptr);
    panel->addApplet(m_vaxApplet);

    // Phase 3M-4 Task 13 — PureSignalApplet quick-access surface.
    //
    // Constructed unconditionally and added to the right panel, but
    // visibility is gated on caps.hasPureSignal in onConnectionStateChanged
    // (HL2 / Atlas hide the applet entirely; G2-class boards show it).
    // Right-click on every PureSignalApplet control opens PsForm via the
    // openPureSignalDialogRequested signal, which MainWindow forwards to
    // openPureSignalDialog (same singleton dialog as Tools / DSP menu).
    m_pureSignalApplet = new PureSignalApplet(m_radioModel, nullptr);
    panel->addApplet(m_pureSignalApplet);
    connect(m_pureSignalApplet,
            &PureSignalApplet::openPureSignalDialogRequested,
            this, &MainWindow::openPureSignalDialog);
    // Initial visibility from current board caps; tracked thereafter via
    // onConnectionStateChanged (where hasPureSignal is also gated on the
    // PSA bottom-banner indicator).
    m_pureSignalApplet->setVisible(
        m_radioModel->boardCapabilities().hasPureSignal);

    // Phase 23: TCI applets — live in Container #0 below the existing applets.
    // Visibility is now managed by AppletVisibilityController below
    // (registered as ids "Tci" + "ClientChain", keys AppletTciVisible +
    // AppletClientChainVisible). Legacy keys TciApplet_Visible /
    // ClientChainApplet_Visible from earlier versions become orphans on
    // upgrade; existing users get TCI applets back to default-visible.
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer) {
        m_tciApplet = new TciApplet(m_tciServer, nullptr);
        panel->addApplet(m_tciApplet);
        connect(m_tciApplet, &TciApplet::setupRequested,
                this, &MainWindow::openTciSetupPage);
        // showClientsRequested: scroll/show the ClientChainApplet.
        // ClientChainApplet is constructed immediately below, so capture
        // by pointer — the lambda runs only after full construction.
        connect(m_tciApplet, &TciApplet::showClientsRequested,
                this, [this]() {
                    if (m_clientChainApplet && m_appletVis) {
                        m_appletVis->setVisible(
                            QStringLiteral("ClientChain"), true);
                        m_clientChainApplet->raise();
                    }
                });

        m_clientChainApplet = new ClientChainApplet(m_tciServer, nullptr);
        panel->addApplet(m_clientChainApplet);
    }
#endif

    // Phase 3P-II Task 20: AmpApplet (PGXL telemetry + OPERATE toggle).
    // Added to the panel alongside the other applets. Signal routing to
    // PgxlConnection is wired in onConnectionStateChanged() so every
    // radio-connect gets a fresh binding without double-connects.
    m_ampApplet = new AmpApplet(m_radioModel, nullptr);
    panel->addApplet(m_ampApplet);

    // Phase 3P-II Task 20: TunerApplet (TGXL controls + relay bars).
    // Was previously commented out ("TODO ATU phase"). Now constructed
    // with the TunerModel* owned by RadioModel so it tracks TGXL state
    // from construction time.
    // Phase 3P-II Phase 4 Task 89: pass RadioModel's shared TuneMemoryStore
    // so saves from the context menu are visible in TgxlAdvancedPage and vice versa.
    m_tunerApplet = new TunerApplet(m_radioModel,
                                    m_radioModel->tunerModel(),
                                    nullptr,
                                    m_radioModel->tuneMemoryStore());
    panel->addApplet(m_tunerApplet);

    // 2026-05-20 bench fix: rescale TunerApplet's fwd-power bar when
    // PGXL comes into the chain. TunerApplet defaults to 0-200 W
    // (barefoot) which pegs out the moment PGXL pushes its amplified
    // ~450-2000 W through the tuner. Mirror the same amplifierChanged
    // + ampStateChanged wires we use for the SMeterWidget above.
    if (m_tunerApplet) {
        // Initial scale: match current PGXL state at construction time.
        const bool amplifyingNow =
            m_radioModel->hasAmplifier() && m_radioModel->ampOperate();
        m_tunerApplet->setPowerScale(/*maxWatts=*/0, amplifyingNow);

        // PGXL connect/disconnect snaps the scale to 2 kW or back to
        // barefoot. maxWatts=0 means "use the standard barefoot/PGXL
        // range from TunerApplet::setPowerScale defaults".
        connect(m_radioModel, &RadioModel::amplifierChanged, this,
                [this](bool present) {
            if (m_tunerApplet) {
                m_tunerApplet->setPowerScale(/*maxWatts=*/0, present);
            }
        });

        // OPERATE/STANDBY edges re-evaluate scale. STANDBY -> barefoot
        // until OPERATE resumes (pass amplifying=false to drop the
        // 2 kW scale back to 200 W).
        connect(m_radioModel, &RadioModel::ampStateChanged, this,
                [this]() {
            if (m_tunerApplet) {
                const bool amplifying = m_radioModel->hasAmplifier()
                                        && m_radioModel->ampOperate();
                m_tunerApplet->setPowerScale(/*maxWatts=*/0, amplifying);
            }
        });
    }

    // Phase 3P-III Task 14: RF-Kit RF2K-S applet.
    // Constructed unconditionally alongside the other peripheral applets.
    // Visibility is gated on rfKitEnabled() via the AppletVisibilityController
    // availability axis (set below, live-updated via rfKitEnabledChanged).
    //
    // Data-flow and context-menu signals are wired once here in buildUI()
    // because rfKitConnection() returns the same permanent object for the
    // app lifetime (RadioModel creates it in its ctor, never destroys it).
    // This differs from the AmpApplet/TunerApplet pattern (wired in
    // onConnectionStateChanged) because PGXL/TGXL use PGX-specific
    // auto-connect logic gated on fourO3AEnabled; the RF-Kit connects
    // independently at startup when rfKitEnabled is true.
    m_rfKitApplet = new Rf2ksApplet(m_radioModel, nullptr);
    panel->addApplet(m_rfKitApplet);

    {
        // Connection -> applet data flow.
        Rf2ksConnection* rfKitConn = m_radioModel->rfKitConnection();
        if (rfKitConn) {
            connect(rfKitConn, &Rf2ksConnection::powerUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setPower);
            connect(rfKitConn, &Rf2ksConnection::tunerUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setTuner);
            connect(rfKitConn, &Rf2ksConnection::antennasUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setAntennas);
            connect(rfKitConn, &Rf2ksConnection::activeAntennaUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setActiveAntenna);
            connect(rfKitConn, &Rf2ksConnection::operateModeUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setOperateMode);
            connect(rfKitConn, &Rf2ksConnection::connected,
                    this, [this]() {
                if (m_rfKitApplet) {
                    m_rfKitApplet->setConnectedState(true);
                }
            });
            connect(rfKitConn, &Rf2ksConnection::disconnected,
                    this, [this]() {
                if (m_rfKitApplet) {
                    m_rfKitApplet->setConnectedState(false);
                }
            });
            connect(rfKitConn, &Rf2ksConnection::infoUpdated,
                    this, [this](const QString& /*deviceName*/,
                                 const QString& softwareVersion,
                                 const QString& nicknameFromAmp) {
                if (m_rfKitApplet) {
                    m_rfKitApplet->setNicknameAndVersion(nicknameFromAmp, softwareVersion);
                }
            });

            // Applet -> connection (antenna click, operate toggle).
            connect(m_rfKitApplet, &Rf2ksApplet::antennaRequested,
                    rfKitConn, &Rf2ksConnection::setActiveAntenna);
            connect(m_rfKitApplet, &Rf2ksApplet::operateToggled,
                    this, [this](bool wantOperate) {
                Rf2ksConnection* conn = m_radioModel->rfKitConnection();
                if (!conn) { return; }
                conn->setOperateMode(wantOperate
                    ? QStringLiteral("OPERATE")
                    : QStringLiteral("STANDBY"));
            });
        }

        // Context menu signals (always wired regardless of connection state).
        connect(m_rfKitApplet, &Rf2ksApplet::navigationRequested,
                this, &MainWindow::openSetup);

        connect(m_rfKitApplet, &Rf2ksApplet::connectionToggleRequested,
                this, [this]() {
            Rf2ksConnection* conn = m_radioModel->rfKitConnection();
            if (!conn) { return; }
            if (conn->isConnected()) {
                conn->disconnect();
            } else {
                // Per-radio peripherals refactor (2026-05-26): host/port
                // live under hardware/<mac>/peripherals/.  When no radio
                // is connected, peripheralValue() returns the default and
                // the empty-host gate below makes this a safe no-op.
                const QString host = m_radioModel->peripheralValue(
                    QStringLiteral("RfKit_ManualIp"));
                const quint16 port = static_cast<quint16>(
                    m_radioModel->peripheralValue(
                        QStringLiteral("RfKit_ManualPort"),
                        QStringLiteral("8080")).toUInt());
                if (!host.isEmpty()) {
                    conn->connectToAmp(host, port);
                }
            }
        });

        connect(m_rfKitApplet, &Rf2ksApplet::diagnosticsCopyRequested,
                this, [this]() {
            Rf2ksConnection* conn = m_radioModel->rfKitConnection();
            QString diag;
            diag += QStringLiteral("RF-Kit RF2K-S diagnostics\n");
            if (conn) {
                diag += QStringLiteral("Host: %1:%2\n")
                            .arg(conn->peerAddress()).arg(conn->peerPort());
                diag += QStringLiteral("Version: %1\n")
                            .arg(conn->softwareVersion());
                diag += QStringLiteral("Polls OK/failed: %1/%2\n")
                            .arg(conn->pollsSucceeded())
                            .arg(conn->pollsFailed());
                diag += QStringLiteral("RTT avg: %1 ms\n")
                            .arg(conn->rttAvgLast10Ms());
            } else {
                diag += QStringLiteral("(connection unavailable)\n");
            }
            QGuiApplication::clipboard()->setText(diag);
        });
    }

    // Antenna labels: load operator-set labels from AppSettings at startup.
    // Keys: RfKit_Ant1_Label .. RfKit_Ant4_Label (stored by RfKitPage.cpp).
    // If a key is absent or empty the applet already shows "ANT N" by default.
    for (int i = 1; i <= 4; ++i) {
        const QString label = AppSettings::instance()
            .value(QStringLiteral("RfKit_Ant%1_Label").arg(i)).toString();
        if (!label.isEmpty()) {
            m_rfKitApplet->setAntennaLabel(i, label);
        }
    }

    // ── Applet visibility controller (Containers > Applets + ☰ menus) ──
    // NereusSDR-original. Backs the show/hide menu surfaces.
    //
    // Registered applets get a checkable menu entry in Containers > Applets
    // AND in the right-side panel's ☰ banner menu. Add new entries here as
    // additional applets ship.
    //
    // RadeApplet caveat: its visibility is mode-driven (auto-shown when
    // the active slice is in DSPMode::RADE_U/_L; hidden otherwise) via
    // the dspModeChanged lambda further down. The menu toggle here is a
    // user override that lasts until the next mode change repopulates
    // visibility. Acceptable for v1; tighter integration is a follow-up.
    m_appletVis = new AppletVisibilityController(this);

    m_appletsById[QStringLiteral("Rx")]         = m_rxApplet;
    m_appletsById[QStringLiteral("Tx")]         = m_txApplet;
    m_appletsById[QStringLiteral("PhoneCw")]    = m_phoneCwApplet;
    m_appletsById[QStringLiteral("Rade")]       = m_radeApplet;
    m_appletsById[QStringLiteral("Vax")]        = m_vaxApplet;
    m_appletsById[QStringLiteral("PureSignal")] = m_pureSignalApplet;
    m_appletsById[QStringLiteral("Amp")]        = m_ampApplet;
    m_appletsById[QStringLiteral("Tuner")]      = m_tunerApplet;
    m_appletsById[QStringLiteral("RfKit")]      = m_rfKitApplet;
#ifdef HAVE_WEBSOCKETS
    if (m_tciApplet) {
        m_appletsById[QStringLiteral("Tci")]        = m_tciApplet;
    }
    if (m_clientChainApplet) {
        m_appletsById[QStringLiteral("ClientChain")] = m_clientChainApplet;
    }
#endif

    // Display names match each applet's appletTitle() — keep in sync if
    // an applet's title changes.
    //
    // PureSignal defaults to visible. The existing onConnectionStateChanged
    // handler still calls m_pureSignalApplet->setVisible(caps.hasPureSignal)
    // on radio connect, which can hide the inner widget on HL2/Atlas; the
    // menu entry stays available for users who want to force-show or
    // permanently hide it. Defaulting to true here means new G2 users see
    // PS immediately without having to discover the menu toggle.
    m_appletVis->registerApplet(QStringLiteral("Rx"),
                                QStringLiteral("RX"),           true);
    m_appletVis->registerApplet(QStringLiteral("Tx"),
                                QStringLiteral("TX"),           true);
    m_appletVis->registerApplet(QStringLiteral("PhoneCw"),
                                QStringLiteral("Phone / CW"),   true);
    // RADE: defaultVisible=true (user pref). Actual visibility is gated
    // on the active slice's mode via the availability axis — the
    // dspModeChanged lambda below calls setAvailable(true) only when
    // mode is DSPMode::RADE_U/_L. Initial availability set to false here
    // since the default startup mode is USB; the mode lambda fires
    // shortly after to correct it if needed.
    m_appletVis->registerApplet(QStringLiteral("Rade"),
                                QStringLiteral("RADE"),         true);
    m_appletVis->registerApplet(QStringLiteral("Vax"),
                                QStringLiteral("VAX"),          true);
    m_appletVis->registerApplet(QStringLiteral("PureSignal"),
                                QStringLiteral("PureSignal"),   true);
    m_appletVis->registerApplet(QStringLiteral("Amp"),
                                QStringLiteral("Power Genius"), true);
    m_appletVis->registerApplet(QStringLiteral("Tuner"),
                                QStringLiteral("Tuner Genius"), true);
    m_appletVis->registerApplet(QStringLiteral("RfKit"),
                                QStringLiteral("RF-Kit RF2K-S"), true);
#ifdef HAVE_WEBSOCKETS
    if (m_tciApplet) {
        m_appletVis->registerApplet(QStringLiteral("Tci"),
                                    QStringLiteral("TCI Server"),  true);
    }
    if (m_clientChainApplet) {
        m_appletVis->registerApplet(QStringLiteral("ClientChain"),
                                    QStringLiteral("TCI Clients"), true);
    }
#endif

    // Capability gates: applets that depend on an external feature flag
    // get their availability set here. When availability is false, the
    // applet is hidden AND its menu entries are greyed out. The user's
    // persisted visibility preference is preserved across availability
    // changes (so re-enabling 4O3A pops the applet back if the user
    // wanted it visible).
    const bool fourO3AOn = m_radioModel && m_radioModel->fourO3AEnabled();
    m_appletVis->setAvailable(QStringLiteral("Amp"),   fourO3AOn);
    m_appletVis->setAvailable(QStringLiteral("Tuner"), fourO3AOn);
    // RADE: available only in RADE_U / RADE_L modes. Startup mode is
    // USB, so initial availability=false. The dspModeChanged lambda
    // below updates this on every mode change.
    m_appletVis->setAvailable(QStringLiteral("Rade"),  false);

    // RF-Kit RF2K-S: available only when the master toggle is enabled.
    // Default OFF; live-updated via rfKitEnabledChanged below.
    const bool rfKitOn = m_radioModel && m_radioModel->rfKitEnabled();
    m_appletVis->setAvailable(QStringLiteral("RfKit"), rfKitOn);

    // Apply initial visibility state from the controller (in case
    // AppSettings already had values from a prior session).
    // Uses effective visibility (user pref AND available).
    for (const QString& id : m_appletVis->registeredIds()) {
        if (auto* a = m_appletsById.value(id, nullptr)) {
            panel->setAppletVisible(a, m_appletVis->isEffectivelyVisible(id));
        }
    }

    // Pump future EFFECTIVE-visibility changes from the controller into
    // the panel. effectiveVisibilityChanged fires when either the user
    // toggle or the availability gate flips the net visibility, so we
    // catch both menu clicks and external capability changes (e.g. 4O3A).
    connect(m_appletVis, &AppletVisibilityController::effectiveVisibilityChanged,
            this, [this](const QString& id, bool effective) {
        if (auto* a = m_appletsById.value(id, nullptr)) {
            if (m_appletPanel) {
                m_appletPanel->setAppletVisible(a, effective);
            }
        }
    });

    // Live-track 4O3A master toggle so Amp/Tuner availability updates
    // without an app restart. RadioModel::setFourO3AEnabled emits the
    // signal whenever the persisted value changes.
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::fourO3AEnabledChanged,
                this, [this](bool enabled) {
            if (!m_appletVis) { return; }
            m_appletVis->setAvailable(QStringLiteral("Amp"),   enabled);
            m_appletVis->setAvailable(QStringLiteral("Tuner"), enabled);
        });

        // Phase 3P-III Task 14: live-track RF-Kit master toggle so the
        // RfKit applet availability updates without an app restart.
        connect(m_radioModel, &RadioModel::rfKitEnabledChanged,
                this, [this](bool enabled) {
            if (!m_appletVis) { return; }
            m_appletVis->setAvailable(QStringLiteral("RfKit"), enabled);
        });
    }

    // ── Banner ☰ menu on AppletPanelWidget ──────────────────────────────
    if (m_appletVis && m_appletPanel) {
        m_bannerAppletsMenu = new QMenu(this);

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = m_bannerAppletsMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // Grey out the entry when the applet is currently unavailable
            // (e.g. Amp/Tuner when 4O3A is disabled). The check state still
            // reflects the user preference so re-enabling restores it.
            act->setEnabled(m_appletVis->isAvailable(id));
            // User-visible tooltip — plain English.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
            });
            m_bannerAppletActions.insert(id, act);
        }

        // Sync banner checkmarks when state changes elsewhere (top menu).
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_bannerAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });

        // Grey/un-grey banner entries when an applet's availability
        // changes (e.g. 4O3A master toggle flipped).
        connect(m_appletVis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString& id, bool available) {
            if (auto* act = m_bannerAppletActions.value(id, nullptr)) {
                act->setEnabled(available);
            }
        });

        m_appletPanel->setBannerMenu(m_bannerAppletsMenu);
    }

    // Ghost applets: constructed but not added to the panel or the Containers menu
    // until their feature phases ship. Uncomment the construction + addContainerToggle
    // call (in buildMenuBar) together when the feature lands.
    //
    // m_digitalApplet    = new DigitalApplet(m_radioModel, nullptr);    // TODO 3-VAX
    // m_diversityApplet  = new DiversityApplet(m_radioModel, nullptr);  // TODO 3F (multi-RX)
    // m_cwxApplet        = new CwxApplet(m_radioModel, nullptr);        // TODO 3M-2 (CW TX)
    // m_dvkApplet        = new DvkApplet(m_radioModel, nullptr);        // TODO 3M-1 (DVK)
    // m_catApplet        = new CatApplet(m_radioModel, nullptr);        // TODO 3J/3K/3-VAX

    c0->setContent(panel);
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
    qCDebug(lcContainer) << "Container #0: Meters + RxApplet + TxApplet + PhoneCwApplet + VaxApplet + TciApplet + ClientChainApplet";
}

void MainWindow::buildMenuBar()
{
    // =========================================================================
    // FILE
    // =========================================================================
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    {
        QAction* settingsAction = fileMenu->addAction(QStringLiteral("&Settings..."),
            this, [this]() {
                auto* dialog = new SetupDialog(m_radioModel, this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                wireSetupDialog(dialog);
                dialog->show();
            });
        settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
        settingsAction->setMenuRole(QAction::NoRole);  // Keep in File menu, don't let macOS move it
        settingsAction->setToolTip(QStringLiteral("Open application settings"));
    }

    {
        QMenu* profilesMenu = fileMenu->addMenu(QStringLiteral("&Profiles"));
        QAction* txProfilesAction = profilesMenu->addAction(QStringLiteral("&TX Profiles..."));
        txProfilesAction->setEnabled(false);
        txProfilesAction->setToolTip(QStringLiteral("NYI — Phase X"));
        QAction* micProfilesAction = profilesMenu->addAction(QStringLiteral("&Mic Profiles..."));
        micProfilesAction->setEnabled(false);
        micProfilesAction->setToolTip(QStringLiteral("NYI — Phase X"));
        profilesMenu->addSeparator();
        QAction* importAction = profilesMenu->addAction(QStringLiteral("&Import..."));
        importAction->setEnabled(false);
        importAction->setToolTip(QStringLiteral("NYI — Phase X"));
        QAction* exportAction = profilesMenu->addAction(QStringLiteral("&Export..."));
        exportAction->setEnabled(false);
        exportAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    fileMenu->addSeparator();

    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q),
                        qApp, &QApplication::quit);

    // =========================================================================
    // RADIO
    // =========================================================================
    // ── Radio menu — 3Q-9: role-based items with state-aware enablement ──────
    QMenu* radioMenu = menuBar()->addMenu(QStringLiteral("&Radio"));

    // Connect (⌘K) — reconnects to the last-used radio. Greyed out when there
    // is no actionable target (currently connected, or no lastConnected MAC,
    // or the lastConnected MAC isn't in saved radios). Manage Radios is the
    // ONLY menu item whose job is to open the Connection Panel; Connect is
    // strictly a one-click reconnect.
    m_actConnect = radioMenu->addAction(QStringLiteral("&Connect"),
        QKeySequence(Qt::CTRL | Qt::Key_K),
        this, [this]() {
            if (m_radioModel->isConnected()) {
                return;
            }
            AppSettings& s = AppSettings::instance();
            const QString lastMac = s.lastConnected();
            const auto saved = s.savedRadio(lastMac);
            if (!saved.has_value()) {
                return;  // enablement should have prevented this
            }
            // Unicast probe targeted at the saved IP — cleaner than a
            // broadcast scan + radioDiscovered listener: no leaked listeners
            // when the radio doesn't reply, and works across VPN tunnels
            // that drop broadcast traffic. Phase 3Q-2 wired probeAddress;
            // this menu item now uses it directly. (Earlier broadcast-listen
            // implementation leaked a connect-on-mac-match listener that
            // would auto-reconnect to LOCAL radio on later scans even after
            // the user explicitly disconnected — bug reported 2026-04-30.)
            RadioDiscovery* disc = m_radioModel->discovery();
            QMetaObject::Connection* connPtr = new QMetaObject::Connection;
            QMetaObject::Connection* failPtr = new QMetaObject::Connection;
            auto cleanup = [connPtr, failPtr]() {
                QObject::disconnect(*connPtr);
                delete connPtr;
                QObject::disconnect(*failPtr);
                delete failPtr;
            };
            *connPtr = connect(disc, &RadioDiscovery::radioDiscovered,
                this, [this, lastMac, cleanup](const RadioInfo& found) {
                    if (found.macAddress != lastMac) {
                        return;  // probe reply for a different radio — wait
                    }
                    if (m_radioModel->isConnected()) {
                        cleanup();
                        return;
                    }
                    cleanup();
                    RadioInfo ri = found;
                    HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
                    if (mo != HPSDRModel::FIRST) {
                        ri.modelOverride = mo;
                    }
                    m_radioModel->connectToRadio(ri);
                });
            *failPtr = connect(disc, &RadioDiscovery::probeFailed,
                this, [cleanup, lastMac](const QHostAddress&, quint16) {
                    qCInfo(lcConnection) << "Connect: probe failed for"
                                         << lastMac;
                    cleanup();
                });
            disc->probeAddress(saved->info.address, saved->info.port);
        });
    m_actConnect->setToolTip(QStringLiteral(
        "Reconnect to the last-used radio (greyed out when there's nothing to reconnect to)"));

    // Disconnect (⌘⇧K) — disabled while disconnected.
    m_actDisconnect = radioMenu->addAction(QStringLiteral("&Disconnect"),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
        this, [this]() { m_radioModel->disconnectFromRadio(); });
    m_actDisconnect->setToolTip(QStringLiteral("Disconnect from the current radio"));

    radioMenu->addSeparator();

    // Manage Radios — always enabled; sole purpose is to open the panel
    // (which has its own ↻ Scan button for fresh broadcast discovery).
    m_actManageRadios = radioMenu->addAction(QStringLiteral("&Manage Radios…"),
        this, &MainWindow::showConnectionPanel);
    m_actManageRadios->setToolTip(QStringLiteral(
        "Open the Connection Panel (radio list + ↻ Scan)"));

    radioMenu->addSeparator();

    {
        QAction* antennaSetupAction = radioMenu->addAction(QStringLiteral("&Antenna Setup…"));
        antennaSetupAction->setEnabled(false);
        antennaSetupAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* transvertersAction = radioMenu->addAction(QStringLiteral("Trans&verters…"));
        transvertersAction->setEnabled(false);
        transvertersAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    radioMenu->addSeparator();

    // Protocol Info — disabled while disconnected; shows a QMessageBox with
    // the connected radio's protocol, firmware, and address info.
    m_actProtocolInfo = radioMenu->addAction(QStringLiteral("&Protocol Info"),
        this, [this]() {
            if (!m_radioModel->isConnected()) {
                return;
            }
            RadioInfo info = m_radioModel->connection()->radioInfo();
            const QString proto =
                info.protocol == ProtocolVersion::Protocol2
                    ? QStringLiteral("P2") : QStringLiteral("P1");
            const QString msg =
                QStringLiteral("Radio:    %1\nProtocol: %2\nFirmware: %3\nMAC:      %4\nIP:       %5")
                    .arg(info.displayName())
                    .arg(proto)
                    .arg(info.firmwareVersion)
                    .arg(info.macAddress, info.address.toString());
            QMessageBox::information(this, QStringLiteral("Protocol Info"), msg);
        });
    m_actProtocolInfo->setToolTip(QStringLiteral(
        "Show connected radio protocol, firmware, and address details"));

    // Initial enablement (before any connectionStateChanged fires). Connect
    // is enabled only when there's a last-used radio in saved entries — i.e.
    // when "reconnect" actually has a target.
    {
        AppSettings& s = AppSettings::instance();
        const QString lastMac = s.lastConnected();
        const bool hasReconnectTarget =
            !lastMac.isEmpty() && s.savedRadio(lastMac).has_value();
        m_actConnect->setEnabled(hasReconnectTarget);
    }
    m_actDisconnect->setEnabled(false);
    m_actProtocolInfo->setEnabled(false);

    // =========================================================================
    // VIEW
    // =========================================================================
    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

    // Phase 3F Sub-Epic D Task 14: live Pan Layout / Add slice / Float
    // entries. Replace the NYI submenu and disabled Add/Remove placeholders
    // with the working stack actions. The bottom-bar +PAN dropdown
    // (Task 10) is kept as the operator-facing primary; these menu items
    // are for operators who prefer the menubar / keyboard shortcuts.
    {
        QAction* panLayoutAct = viewMenu->addAction(QStringLiteral("Pan &Layout…"));
        panLayoutAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
        panLayoutAct->setToolTip(QStringLiteral(
            "Pick a panadapter layout template (1 / 2v / 2h / 12h / 2x2)"));
        connect(panLayoutAct, &QAction::triggered, this, [this]() {
            if (!m_panStack) { return; }
            PanLayoutDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                applyPanLayout(dialog.selectedLayout());
            }
        });
    }

    {
        QAction* addSliceAct = viewMenu->addAction(
            QStringLiteral("&Add slice on active pan"));
        addSliceAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
        addSliceAct->setToolTip(QStringLiteral(
            "Create a new slice on the active panadapter (up to maxSlices())"));
        connect(addSliceAct, &QAction::triggered, this, [this]() {
            if (m_panStack && m_radioModel) {
                m_radioModel->addSliceOnPan(m_panStack->activePanId());
            }
        });
    }

    {
        QAction* floatAct = viewMenu->addAction(
            QStringLiteral("&Float active pan…"));
        floatAct->setToolTip(QStringLiteral(
            "Detach the active panadapter into its own floating window"));
        connect(floatAct, &QAction::triggered, this, [this]() {
            if (m_panStack) {
                m_panStack->floatPanadapter(m_panStack->activePanId());
            }
        });
    }

    viewMenu->addSeparator();

    // From AetherSDR MainWindow.cpp:4098-4130 [@0cd4559]
    {
        QMenu* bandPlanMenu = viewMenu->addMenu(QStringLiteral("&Band Plan"));

        const int savedBpSize = AppSettings::instance()
                                    .value(QStringLiteral("BandPlanFontSize"),
                                           QStringLiteral("6"))
                                    .toInt();

        QActionGroup* bpGroup = new QActionGroup(bandPlanMenu);
        bpGroup->setExclusive(true);
        struct BpOption { const char* label; int pt; };
        const BpOption bpModes[] = {
            { "&Off",    0  },
            { "&Small",  6  },
            { "&Medium", 10 },
            { "&Large",  12 },
            { "&Huge",   16 },
        };
        for (const auto& opt : bpModes) {
            QAction* a = bandPlanMenu->addAction(QString::fromUtf8(opt.label));
            a->setCheckable(true);
            a->setChecked(opt.pt == savedBpSize);
            bpGroup->addAction(a);
            const int pt = opt.pt;
            connect(a, &QAction::triggered, this, [this, pt]() {
                if (activeSpectrumWidget()) {
                    activeSpectrumWidget()->setBandPlanFontSize(pt);
                }
                AppSettings::instance().setValue(QStringLiteral("BandPlanFontSize"),
                                                 QString::number(pt));
            });
        }

        bandPlanMenu->addSeparator();

        QActionGroup* planGroup = new QActionGroup(bandPlanMenu);
        planGroup->setExclusive(true);
        const auto& mgr = m_radioModel->bandPlanManager();
        const QString activePlan = mgr.activePlanName();
        for (const QString& name : mgr.availablePlans()) {
            QAction* a = bandPlanMenu->addAction(name);
            a->setCheckable(true);
            a->setChecked(name == activePlan);
            planGroup->addAction(a);
            connect(a, &QAction::triggered, this, [this, name]() {
                m_radioModel->bandPlanManagerMutable().setActivePlan(name);
            });
        }
    }

    {
        QMenu* displayModeMenu = viewMenu->addMenu(QStringLiteral("&Display Mode"));
        QAction* placeholder = displayModeMenu->addAction(QStringLiteral("(NYI placeholder)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    {
        QMenu* uiScaleMenu = viewMenu->addMenu(QStringLiteral("&UI Scale"));
        QActionGroup* scaleGroup = new QActionGroup(this);
        scaleGroup->setExclusive(true);
        const struct { const char* label; bool isDefault; } scales[] = {
            { "&75%",  false },
            { "&100%", true  },
            { "&125%", false },
            { "&150%", false },
            { "&175%", false },
            { "&200%", false },
        };
        for (const auto& s : scales) {
            QAction* a = uiScaleMenu->addAction(QString::fromUtf8(s.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase X"));
            if (s.isDefault) { a->setChecked(true); }
            scaleGroup->addAction(a);
        }
    }

    // Phase 23 "View > Network Applets" submenu removed: TCI Server and
    // TCI Clients are now driven by AppletVisibilityController, accessible
    // via Containers > Applets and the panel's ☰ banner menu. Two
    // independent controls (old direct-setVisible + new controller path)
    // would drift out of sync. CAT + MIDI greyed placeholders deferred to
    // their feature phases (3K-1 / 3K-3) — re-add at that time wired
    // through the controller.

    viewMenu->addSeparator();

    m_darkThemeAction = viewMenu->addAction(QStringLiteral("&Dark Theme"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(true);
    m_darkThemeAction->setToolTip(QStringLiteral("Toggle dark theme (NYI — Phase X)"));
    connect(m_darkThemeAction, &QAction::toggled, this, [](bool /*on*/) {
        qCDebug(lcConnection) << "Dark Theme toggle NYI";
    });

    {
        QAction* minimalAction = viewMenu->addAction(QStringLiteral("&Minimal Mode"));
        minimalAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
        minimalAction->setEnabled(false);
        minimalAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // 2026-05-26 KG4VCF perf instrumentation: toggle the in-spectrum
    // perf overlay (paint/gap/fft/overlay timings + audio underruns
    // + UDP drops + memory pressure).  Persisted via AppSettings
    // "ShowPerfOverlay" by SpectrumWidget::setShowPerfOverlay; the
    // menu item just exposes the toggle.
    {
        QAction* perfAction = viewMenu->addAction(
            QStringLiteral("&Performance Overlay"));
        perfAction->setCheckable(true);
        perfAction->setChecked(activeSpectrumWidget() && activeSpectrumWidget()->showPerfOverlay());
        perfAction->setToolTip(QStringLiteral(
            "Show paint/gap/fft/overlay timings + audio underruns + UDP drops"
            " + memory pressure in a corner of the spectrum panel."
            "  Useful for diagnosing jitter under system load."));
        connect(perfAction, &QAction::toggled, this, [this](bool on) {
            if (activeSpectrumWidget()) {
                activeSpectrumWidget()->setShowPerfOverlay(on);
            }
        });
    }

    viewMenu->addSeparator();

    {
        QAction* kbAction = viewMenu->addAction(QStringLiteral("&Keyboard Shortcuts..."));
        kbAction->setEnabled(false);
        kbAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // DSP
    // =========================================================================
    QMenu* dspMenu = menuBar()->addMenu(QStringLiteral("&DSP"));

    // ── NR submenu — full slot bank, mutual exclusion via QActionGroup ─────
    // Mirrors VfoWidget's 7-button NR bank. Off/NR1/NR2/NR3/NR4/DFNR are
    // always present; MNR is gated by HAVE_MNR (macOS only) and BNR by
    // HAVE_BNR (NVIDIA build, currently never defined). Hidden actions
    // remain in the group so the activeNrChanged sync handler can find
    // them by index.
    {
        QMenu* nrMenu = dspMenu->addMenu(QStringLiteral("&NR"));
        m_nrGroup = new QActionGroup(this);
        m_nrGroup->setExclusive(true);

        using Slot = NereusSDR::NrSlot;
        struct Entry { const char* label; Slot slot; bool hidden; };
        const Entry nrSlots[] = {
            { "&Off",   Slot::Off,  false },
            { "NR&1",   Slot::NR1,  false },
            { "NR&2",   Slot::NR2,  false },
            { "NR&3",   Slot::NR3,  false },
            { "NR&4",   Slot::NR4,  false },
            { "&DFNR",  Slot::DFNR, false },
            { "&MNR",   Slot::MNR,
#ifdef HAVE_MNR
                false
#else
                true
#endif
            },
            { "&BNR",   Slot::BNR,
#ifdef HAVE_BNR
                false
#else
                true
#endif
            },
        };
        for (const auto& nr : nrSlots) {
            Slot slot = nr.slot;
            QAction* a = nrMenu->addAction(QString::fromUtf8(nr.label),
                this, [this, slot]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setActiveNr(slot); }
                });
            a->setCheckable(true);
            if (nr.hidden) { a->setVisible(false); }
            m_nrGroup->addAction(a);
        }
    }

    // ── NB submenu — Off/NB/NB2 mutual exclusion ───────────────────────────
    // Maps to SliceModel::setNbMode(NbMode). Mirrors VfoWidget's cycling
    // NB button (Off → NB → NB2 → Off) but as discrete menu items.
    {
        QMenu* nbMenu = dspMenu->addMenu(QStringLiteral("N&B"));
        m_nbGroup = new QActionGroup(this);
        m_nbGroup->setExclusive(true);

        using Mode = NereusSDR::NbMode;
        const struct { const char* label; Mode mode; } nbModes[] = {
            { "&Off",  Mode::Off },
            { "&NB",   Mode::NB  },
            { "NB&2",  Mode::NB2 },
        };
        for (const auto& nb : nbModes) {
            Mode mode = nb.mode;
            QAction* a = nbMenu->addAction(QString::fromUtf8(nb.label),
                this, [this, mode]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setNbMode(mode); }
                });
            a->setCheckable(true);
            m_nbGroup->addAction(a);
        }
    }

    // ── Single-toggle DSP actions ──────────────────────────────────────────
    // ANF now goes through SliceModel::anfEnabled (Phase 3F Sub-Epic J
    // Task 3) and is kept in sync below. SNB / APF / BIN go through
    // SliceModel and are synced too.
    {
        QAction* anfAction = dspMenu->addAction(QStringLiteral("&ANF"));
        anfAction->setCheckable(true);
        connect(anfAction, &QAction::toggled, this, [this](bool on) {
            // A control attached to no flag targets the active slice, which
            // is whichever flag the operator last clicked. Resolved at
            // invocation, not captured, so it follows focus.
            if (SliceModel* slice = m_radioModel->activeSlice()) {
                slice->setAnfEnabled(on);
            }
        });

        // Reflect the active slice when focus moves, without re-emitting
        // toggled back into the handler above.
        connect(m_radioModel, &RadioModel::activeSliceChanged, this,
                [this, anfAction](int) {
            if (SliceModel* slice = m_radioModel->activeSlice()) {
                QSignalBlocker block(anfAction);
                anfAction->setChecked(slice->anfEnabled());
            }
        });
    }

    m_snbAction = dspMenu->addAction(QStringLiteral("&SNB"));
    m_snbAction->setCheckable(true);
    connect(m_snbAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setSnbEnabled(on); }
    });

    m_apfAction = dspMenu->addAction(QStringLiteral("AP&F"));
    m_apfAction->setCheckable(true);
    connect(m_apfAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setApfEnabled(on); }
    });

    m_binAction = dspMenu->addAction(QStringLiteral("B&IN"));
    m_binAction->setCheckable(true);
    connect(m_binAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setBinauralEnabled(on); }
    });

    {
        QAction* tnfAction = dspMenu->addAction(QStringLiteral("&TNF"));
        tnfAction->setCheckable(true);
        tnfAction->setEnabled(false);
        tnfAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    dspMenu->addSeparator();

    {
        // AGC submenu — checkable exclusive via QActionGroup.
        // AGCMode enum from WdspTypes.h: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
        // From Thetis dsp.cs AGCMode — all 6 modes wired to SliceModel::setAgcMode().
        QMenu* agcMenu = dspMenu->addMenu(QStringLiteral("&AGC"));
        m_agcGroup = new QActionGroup(this);
        m_agcGroup->setExclusive(true);

        const struct { const char* label; AGCMode mode; } agcModes[] = {
            { "&Off",    AGCMode::Off    },
            { "&Long",   AGCMode::Long   },
            { "&Slow",   AGCMode::Slow   },
            { "&Med",    AGCMode::Med    },
            { "&Fast",   AGCMode::Fast   },
            { "&Custom", AGCMode::Custom },
        };
        for (const auto& agc : agcModes) {
            AGCMode agcMode = agc.mode;
            QAction* a = agcMenu->addAction(QString::fromUtf8(agc.label),
                this, [this, agcMode]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setAgcMode(agcMode); }
                });
            a->setCheckable(true);
            m_agcGroup->addAction(a);
        }

        // Sync AGC checked state when SliceModel changes
        connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
            if (index != 0) { return; }
            SliceModel* slice = m_radioModel->activeSlice();
            if (!slice) { return; }
            connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
                QList<QAction*> acts = m_agcGroup->actions();
                const AGCMode agcOrder[] = {
                    AGCMode::Off, AGCMode::Long, AGCMode::Slow,
                    AGCMode::Med, AGCMode::Fast, AGCMode::Custom
                };
                for (int i = 0; i < acts.size() && i < 6; ++i) {
                    acts[i]->setChecked(agcOrder[i] == mode);
                }
            });
        });
    }

    // ── Sync NR / NB / SNB / APF / BIN checked state from slice 0 ──────────
    // Mirrors the AGC sync pattern above. Wired via sliceAdded so the
    // connection survives slice teardown/re-create. ANF is not synced here:
    // its check state follows activeSliceChanged instead (wired beside the
    // action's creation above), since it must track whichever slice is
    // active, not just slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }

        // NR submenu sync — actions appended to m_nrGroup in this fixed order.
        const NereusSDR::NrSlot nrOrder[] = {
            NereusSDR::NrSlot::Off,  NereusSDR::NrSlot::NR1,
            NereusSDR::NrSlot::NR2,  NereusSDR::NrSlot::NR3,
            NereusSDR::NrSlot::NR4,  NereusSDR::NrSlot::DFNR,
            NereusSDR::NrSlot::MNR,  NereusSDR::NrSlot::BNR,
        };
        auto syncNr = [this, nrOrder](NereusSDR::NrSlot slot) {
            QList<QAction*> acts = m_nrGroup->actions();
            const int n = static_cast<int>(std::size(nrOrder));
            for (int i = 0; i < acts.size() && i < n; ++i) {
                QSignalBlocker b(acts[i]);
                acts[i]->setChecked(nrOrder[i] == slot);
            }
        };
        syncNr(slice->activeNr());
        connect(slice, &SliceModel::activeNrChanged, this, syncNr);

        // NB submenu sync — three actions in m_nbGroup: Off, NB, NB2.
        const NereusSDR::NbMode nbOrder[] = {
            NereusSDR::NbMode::Off, NereusSDR::NbMode::NB, NereusSDR::NbMode::NB2,
        };
        auto syncNb = [this, nbOrder](NereusSDR::NbMode mode) {
            QList<QAction*> acts = m_nbGroup->actions();
            for (int i = 0; i < acts.size() && i < 3; ++i) {
                QSignalBlocker b(acts[i]);
                acts[i]->setChecked(nbOrder[i] == mode);
            }
        };
        syncNb(slice->nbMode());
        connect(slice, &SliceModel::nbModeChanged, this, syncNb);

        // Single-toggle initial sync.
        { QSignalBlocker b(m_snbAction); m_snbAction->setChecked(slice->snbEnabled()); }
        { QSignalBlocker b(m_apfAction); m_apfAction->setChecked(slice->apfEnabled()); }
        { QSignalBlocker b(m_binAction); m_binAction->setChecked(slice->binauralEnabled()); }

        connect(slice, &SliceModel::snbEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_snbAction);
            m_snbAction->setChecked(v);
        });
        connect(slice, &SliceModel::apfEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_apfAction);
            m_apfAction->setChecked(v);
        });
        connect(slice, &SliceModel::binauralEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_binAction);
            m_binAction->setChecked(v);
        });
    });

    dspMenu->addSeparator();

    {
        QAction* eqAction = dspMenu->addAction(QStringLiteral("&Equalizer..."));
        eqAction->setEnabled(false);
        eqAction->setToolTip(QStringLiteral("NYI — Phase 3I-3"));
    }
    {
        // Phase 3M-4 Task 8: wire DSP > PureSignal... to the modeless dialog.
        // Both this entry and Tools > PureSignal... below open the same
        // singleton dialog (DSP for discoverability under the existing
        // DSP-feature menu, Tools per the per-task plan §8.4).
        QAction* psAction = dspMenu->addAction(QStringLiteral("&PureSignal..."));
        psAction->setToolTip(
            QStringLiteral("Open the PureSignal pre-distortion control dialog."));
        connect(psAction, &QAction::triggered,
                this, &MainWindow::openPureSignalDialog);
    }
    {
        QAction* divAction = dspMenu->addAction(QStringLiteral("&Diversity..."));
        divAction->setEnabled(false);
        divAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // BAND
    // =========================================================================
    QMenu* bandMenu = menuBar()->addMenu(QStringLiteral("&Band"));

    {
        QMenu* hfMenu = bandMenu->addMenu(QStringLiteral("&HF"));
        // Frequency values from Thetis console.cs band definitions
        const struct { const char* label; double freqHz; } hfBands[] = {
            { "160m (1.8 MHz)",    1.8e6   },
            { "80m (3.5 MHz)",     3.5e6   },
            { "60m (5.3 MHz)",     5.3e6   },
            { "40m (7.0 MHz)",     7.0e6   },
            { "30m (10.1 MHz)",   10.1e6   },
            { "20m (14.0 MHz)",   14.0e6   },
            { "17m (18.068 MHz)", 18.068e6 },
            { "15m (21.0 MHz)",   21.0e6   },
            { "12m (24.89 MHz)",  24.89e6  },
            { "10m (28.0 MHz)",   28.0e6   },
            { "6m (50.0 MHz)",    50.0e6   },
        };
        for (const auto& band : hfBands) {
            double freq = band.freqHz;
            hfMenu->addAction(QString::fromUtf8(band.label), this, [this, freq]() {
                SliceModel* slice = m_radioModel->activeSlice();
                if (slice) { slice->setFrequency(freq); }
            });
        }
    }

    {
        QMenu* vhfMenu = bandMenu->addMenu(QStringLiteral("&VHF"));
        QAction* placeholder = vhfMenu->addAction(QStringLiteral("(NYI — Phase X)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("VHF bands NYI — Phase X"));
    }

    {
        QMenu* genMenu = bandMenu->addMenu(QStringLiteral("&GEN"));
        QAction* placeholder = genMenu->addAction(QStringLiteral("(NYI — Phase X)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("GEN coverage NYI — Phase X"));
    }

    bandMenu->addAction(QStringLiteral("&WWV (10.0 MHz)"), this, [this]() {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setFrequency(10.0e6); }
    });

    bandMenu->addSeparator();

    {
        QAction* bandStackAction = bandMenu->addAction(QStringLiteral("Band &Stacking..."));
        bandStackAction->setEnabled(false);
        bandStackAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // MODE
    // =========================================================================
    QMenu* modeMenu = menuBar()->addMenu(QStringLiteral("&Mode"));

    // 12 Thetis-faithful modes + the NereusSDR-native RADE-U / RADE-L
    // entries (Phase 3R L3).  Display order: LSB, USB, DSB, CWL, CWU,
    // AM, SAM, FM, DIGL, DIGU, DRM, SPEC, RADE-U, RADE-L.  Maps to
    // DSPMode enum values from WdspTypes.h.
    // From Thetis dsp.cs DSPMode enum — enum values used directly, not indices.
    // RADE-U / RADE-L are NereusSDR-native entries (DSPMode::RADE_U = 12,
    // DSPMode::RADE_L = 13; not WDSP modes; routes the slice through
    // RadeChannel).  Like USB/LSB, RADE has upper/lower sideband
    // variants with mirrored 1700 Hz passbands.
    const struct { const char* label; DSPMode mode; } modes[] = {
        { "LSB",    DSPMode::LSB    },
        { "USB",    DSPMode::USB    },
        { "DSB",    DSPMode::DSB    },
        { "CWL",    DSPMode::CWL    },
        { "CWU",    DSPMode::CWU    },
        { "AM",     DSPMode::AM     },
        { "SAM",    DSPMode::SAM    },
        { "FM",     DSPMode::FM     },
        { "DIGL",   DSPMode::DIGL   },
        { "DIGU",   DSPMode::DIGU   },
        { "DRM",    DSPMode::DRM    },
        { "SPEC",   DSPMode::SPEC   },
        { "RADE-U", DSPMode::RADE_U },  // Phase 3R L3, NereusSDR-native upper
        { "RADE-L", DSPMode::RADE_L },  // Phase 3R L3, NereusSDR-native lower
    };

    m_modeActionGroup = new QActionGroup(this);
    m_modeActionGroup->setExclusive(true);

    for (int i = 0; i < 14; ++i) {
        DSPMode mode = modes[i].mode;
        QAction* act = modeMenu->addAction(QString::fromUtf8(modes[i].label),
                                           this, [this, mode]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setDspMode(mode); }
        });
        act->setCheckable(true);
        m_modeActionGroup->addAction(act);
        m_modeActions[i] = act;
    }

    // Sync checked mode action when SliceModel reports a mode change.
    // Connection is deferred until slice 0 is available (sliceAdded signal).
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }
        connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
            const DSPMode displayOrder[] = {
                DSPMode::LSB, DSPMode::USB, DSPMode::DSB, DSPMode::CWL,
                DSPMode::CWU, DSPMode::AM,  DSPMode::SAM,  DSPMode::FM,
                DSPMode::DIGL, DSPMode::DIGU, DSPMode::DRM, DSPMode::SPEC,
                DSPMode::RADE_U,  // Phase 3R L3, index 12
                DSPMode::RADE_L,  // Phase 3R L3, index 13
            };
            for (int i = 0; i < 14; ++i) {
                if (m_modeActions[i]) {
                    m_modeActions[i]->setChecked(displayOrder[i] == mode);
                }
            }
        });
    });

    // =========================================================================
    // CONTAINERS
    // =========================================================================
    QMenu* containersMenu = menuBar()->addMenu(QStringLiteral("Contai&ners"));

    {
        // New Container: creates a floating container with a fresh MeterWidget,
        // then opens the settings dialog so the user can pick a preset or add items.
        // From Thetis setup.cs:24358 — btnAddRX1Container_Click → AddMeterContainer(1, false)
        QAction* newContAction = containersMenu->addAction(QStringLiteral("&New Container..."));
        connect(newContAction, &QAction::triggered, this, [this]() {
            if (!m_containerManager) { return; }

            ContainerWidget* c = m_containerManager->createContainer(1, DockMode::Floating);
            c->setNotes(QStringLiteral("Meter"));

            // Give it a MeterWidget as content (replaces the default placeholder label)
            MeterWidget* meter = new MeterWidget();
            c->setContent(meter);

            // Open settings dialog so user can configure it
            ContainerSettingsDialog dialog(c, this, m_containerManager);
            if (dialog.exec() == QDialog::Rejected) {
                // User cancelled — destroy the container
                m_containerManager->destroyContainer(c->id());
            }
        });
    }
    {
        // Phase 3G-6 block 6 commit 45: dynamic "Edit Container ▸"
        // submenu populated from ContainerManager::allContainers().
        // Replaces the old static "Container Settings..." action that
        // could only edit Container #0. Rebuilds on
        // containerAdded / containerRemoved / containerTitleChanged
        // signals so menu entries stay in sync with the live
        // container set.
        m_editContainerMenu = containersMenu->addMenu(
            QStringLiteral("&Edit Container"));
        rebuildEditContainerSubmenu();
        if (m_containerManager) {
            connect(m_containerManager, &ContainerManager::containerAdded,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerRemoved,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerTitleChanged,
                    this, [this](const QString&, const QString&) {
                rebuildEditContainerSubmenu();
            });
        }
    }
    {
        // Phase 3G-6 block 6 commit 46: Reset Default Layout — now
        // functional. Destroys every non-panel container and
        // rebuilds the submenu.
        QAction* resetAction = containersMenu->addAction(QStringLiteral("&Reset Default Layout"));
        connect(resetAction, &QAction::triggered, this,
                &MainWindow::resetDefaultLayout);
    }

    containersMenu->addSeparator();

    // ── Containers > Applets section ─────────────────────────────────────
    // Show/hide toggles for each currently-wired applet. Backed by
    // m_appletVis (AppletVisibilityController). Two-way sync with the
    // ☰ menu on AppletPanelWidget happens via the controller's
    // visibilityChanged signal.
    //
    // Predecessor: dead lambda was disabled in 25597df because its 7
    // entries were all ghost applets. The new section ships only
    // currently-wired applets. Add new entries here as additional
    // applets ship (default visible per design §5.2).
    if (m_appletVis) {
        // Section header. addSection is the idiomatic Qt API; falls back
        // gracefully on platforms where it renders as a plain label.
        containersMenu->addSection(QStringLiteral("Applets"));

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = containersMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // Grey out when the applet is currently unavailable (e.g.
            // Amp/Tuner when 4O3A is disabled). Check state still
            // reflects the user preference.
            act->setEnabled(m_appletVis->isAvailable(id));
            // User-visible tooltip — plain English, no source cites.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
            });
            m_topMenuAppletActions.insert(id, act);
        }

        // Sync checkmark when the controller's state changes (e.g. via
        // the banner ☰ menu in Task 6). QSignalBlocker prevents
        // recursive toggle.
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });

        // Grey/un-grey top-menu entries when an applet's availability
        // changes (e.g. 4O3A master toggle flipped in Setup).
        connect(m_appletVis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString& id, bool available) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                act->setEnabled(available);
            }
        });
    }

    // =========================================================================
    // TOOLS
    // =========================================================================
    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    // Phase 3J-2 H1: Spot Hub (DX cluster / RBN / POTA / WSJT-X / FreeDV /
    // PSK Reporter). Modeless singleton dialog; lazy-constructed in
    // openSpotHub() with all 7 clients + SpotModel + DxccColorProvider
    // injected from RadioModel.
    {
        QAction* spotHubAction = toolsMenu->addAction(QStringLiteral("Spot &Hub..."));
        spotHubAction->setObjectName(QStringLiteral("actSpotHub"));
        spotHubAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
        spotHubAction->setToolTip(QStringLiteral(
            "Open the Spot Hub dialog (DX cluster, RBN, POTA, WSJT-X, "
            "FreeDV Reporter, PSK Reporter, Spot Collector)."));
        connect(spotHubAction, &QAction::triggered, this, &MainWindow::openSpotHub);
    }

    // Phase 3J-2 H1: FreeDV Reporter live station map.
    // Modeless singleton dialog; lazy-constructed in openFreeDVReporter()
    // with FreeDVStationModel + FreeDVReporterClient from RadioModel.
    {
        QAction* fdvAction = toolsMenu->addAction(QStringLiteral("&FreeDV Reporter..."));
        fdvAction->setObjectName(QStringLiteral("actFreeDVReporter"));
        fdvAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
        fdvAction->setToolTip(QStringLiteral(
            "Open the FreeDV Reporter dialog (live stations on qso.freedv.org)."));
        connect(fdvAction, &QAction::triggered, this, &MainWindow::openFreeDVReporter);
    }

    toolsMenu->addSeparator();

    // TX Equalizer: modeless singleton dialog (Phase 3M-3a-i Batch 3 A.1).
    {
        QAction* txEqAction = toolsMenu->addAction(QStringLiteral("TX &Equalizer..."));
        txEqAction->setToolTip(QStringLiteral(
            "Open the 10-band TX EQ dialog (preamp + 10 band gains + center frequencies)."));
        connect(txEqAction, &QAction::triggered, this, [this]() {
            if (!m_radioModel) { return; }
            TxEqDialog* dlg = TxEqDialog::instance(m_radioModel, this);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    // Phase 3M-4 Task 8: PureSignal — modeless singleton dialog.
    // Same target as DSP > PureSignal... above; this entry per design doc
    // §4 #3 ("Tools > PureSignal..." for higher discoverability than the
    // DSP-buried path).
    {
        m_actPureSignal = toolsMenu->addAction(QStringLiteral("&PureSignal..."));
        m_actPureSignal->setToolTip(QStringLiteral(
            "Open the PureSignal pre-distortion control dialog."));
        connect(m_actPureSignal, &QAction::triggered,
                this, &MainWindow::openPureSignalDialog);
    }

    // Phase 3F Sub-Epic G T4: Diversity dialog (bench minimum).
    // Lazy-singleton: one dialog instance per RadioModel, kept alive
    // across close so the dialog's own state survives a re-open
    // (SliceModel persistence handles real settings round-trip in T2).
    {
        QAction* divDlgAct = toolsMenu->addAction(QStringLiteral("&Diversity..."));
        divDlgAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
        divDlgAct->setToolTip(QStringLiteral(
            "Open the Diversity dialog (Slice A: enable, phase, gain)."));
        connect(divDlgAct, &QAction::triggered, this, [this]() {
            static DiversityDialog* dlg = nullptr;
            if (!dlg) {
                dlg = new DiversityDialog(m_radioModel, this);
                dlg->setAttribute(Qt::WA_DeleteOnClose, false);
            }
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    toolsMenu->addSeparator();

    {
        QAction* cwxAction = toolsMenu->addAction(QStringLiteral("C&WX..."));
        cwxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
        cwxAction->setEnabled(false);
        cwxAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* memAction = toolsMenu->addAction(QStringLiteral("&Memory Manager..."));
        memAction->setEnabled(false);
        memAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* catAction = toolsMenu->addAction(QStringLiteral("&CAT Control..."));
        catAction->setEnabled(false);
        catAction->setToolTip(QStringLiteral("NYI — Phase 3K"));
    }
    {
        // Phase 23: TCI Server action — enabled, opens Setup → TCI Server.
        QAction* tciAction = toolsMenu->addAction(QStringLiteral("&TCI Server..."));
        tciAction->setToolTip(QStringLiteral("Open TCI Server Setup"));
        connect(tciAction, &QAction::triggered, this, &MainWindow::openTciSetupPage);
    }
    {
        QAction* daxAction = toolsMenu->addAction(QStringLiteral("&VAX Audio..."));
        daxAction->setEnabled(false);
        daxAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* midiAction = toolsMenu->addAction(QStringLiteral("&MIDI Mapping..."));
        midiAction->setEnabled(false);
        midiAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* macroAction = toolsMenu->addAction(QStringLiteral("Macro &Buttons..."));
        macroAction->setEnabled(false);
        macroAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    toolsMenu->addSeparator();

    {
        QAction* netDiagAction = toolsMenu->addAction(QStringLiteral("&Network Diagnostics..."));
        netDiagAction->setEnabled(false);
        netDiagAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    toolsMenu->addAction(QStringLiteral("&Support Bundle..."), this,
                         &MainWindow::showSupportDialog);

    // Phase 3F closeout — operator-visible test entries for Sub-Epic E
    // consumer surfaces. Lets the user verify the toast and TX-bound
    // re-route dialog render correctly without needing to trigger a real
    // antenna conflict. Removed when full conflict-detection state machine
    // lands and the test surfaces become unnecessary.
    toolsMenu->addSeparator();
    {
        QAction* testToastAct = toolsMenu->addAction(
            QStringLiteral("Test antenna switch &toast"));
        testToastAct->setToolTip(
            QStringLiteral("Phase 3F closeout: fire the AntennaSwitchToast surface "
                            "for visual verification. Real auto-switch firing wires "
                            "when the conflict-detection state machine ships."));
        connect(testToastAct, &QAction::triggered, this, [this]() {
            if (m_radioModel) {
                m_radioModel->emitAntennaAutoSwitched(
                    0, QStringLiteral("ANT1"), QStringLiteral("ANT2"));
            }
        });
    }
    {
        QAction* testReRouteAct = toolsMenu->addAction(
            QStringLiteral("Test TX-bound &re-route dialog"));
        testReRouteAct->setToolTip(
            QStringLiteral("Phase 3F closeout: open the TxBoundConfirmDialog surface "
                            "for visual verification. Real emission from addSliceOnPan "
                            "wires when the conflict-detection state machine ships."));
        connect(testReRouteAct, &QAction::triggered, this, [this]() {
            if (m_radioModel) {
                m_radioModel->requestTxBoundReRoute(
                    QStringLiteral("ANT2"), QStringLiteral("ANT1"));
            }
        });
    }

    // =========================================================================
    // HELP
    // =========================================================================
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    {
        QAction* gettingStartedAction = helpMenu->addAction(QStringLiteral("&Getting Started"));
        gettingStartedAction->setEnabled(false);
        gettingStartedAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* helpAction = helpMenu->addAction(QStringLiteral("&NereusSDR Help"));
        helpAction->setEnabled(false);
        helpAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* dataModesAction = helpMenu->addAction(QStringLiteral("Understanding &Data Modes"));
        dataModesAction->setEnabled(false);
        dataModesAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    helpMenu->addSeparator();

    {
        QAction* whatsNewAction = helpMenu->addAction(QStringLiteral("What's &New"));
        whatsNewAction->setEnabled(false);
        whatsNewAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    helpMenu->addSeparator();

#if defined(Q_OS_LINUX)
    // PipeWire / pactl diagnostic dialog — Linux-only feature.
    helpMenu->addAction(QStringLiteral("&Diagnose audio backend…"),
                        this, &MainWindow::showAudioDiagnoseDialog);

    helpMenu->addSeparator();
#endif

    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });

    // Phase 3J-2 H1: Ctrl+Shift+K clears all rows in SpotModel. Mirrors the
    // "Clear All Spots" button on SpotHubDialog's Display tab so the user
    // can wipe stale spots without opening the dialog. Application-scoped
    // QShortcut so it fires regardless of which child widget has focus.
    {
        auto* clearSpotsShortcut = new QShortcut(
            QKeySequence(QStringLiteral("Ctrl+Shift+K")), this);
        clearSpotsShortcut->setContext(Qt::ApplicationShortcut);
        connect(clearSpotsShortcut, &QShortcut::activated, this, [this]() {
            if (m_radioModel && m_radioModel->spotModel()) {
                m_radioModel->spotModel()->clear();
            }
        });
    }
}

void MainWindow::buildStatusBar()
{
    // AetherSDR double-height status bar (46px fixed height, 3-section layout)
    QStatusBar* sb = statusBar();
    sb->setFixedHeight(46);
    sb->setSizeGripEnabled(false);
    sb->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #0a0a14; border-top: 1px solid #203040; }"
        "QStatusBar::item { border: none; }"));

    // Wrapper widget for the full-width custom layout. Stored as a
    // member so reapplyRightStripDropPriority() can read its width.
    m_chromeBarWidget = new QWidget(sb);
    m_chromeBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QWidget* barWidget = m_chromeBarWidget;   // local alias keeps existing code below tidy
    QHBoxLayout* hbox = new QHBoxLayout(barWidget);
    hbox->setContentsMargins(6, 0, 6, 0);
    hbox->setSpacing(6);

    // Helper: styled separator dot
    auto makeSep = [&]() -> QLabel* {
        auto* sep = new QLabel(QStringLiteral(" · "), barWidget);
        sep->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 18px; }"));
        return sep;
    };

    // ── Left section ──────────────────────────────────────────────────────────

    // Band Stack: three grey circles (NYI — clickable placeholder)
    auto* bandStackLabel = new QLabel(barWidget);
    bandStackLabel->setFixedSize(10, 22);
    {
        QPixmap pm(10, 22);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(0x40, 0x48, 0x58));
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            painter.drawEllipse(0, i * 7, 9, 6);
        }
        painter.end();
        bandStackLabel->setPixmap(pm);
    }
    bandStackLabel->setToolTip(QStringLiteral("Band Stack (NYI)"));
    bandStackLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(bandStackLabel);

    // Phase 3F Sub-Epic D Task 10: activated +PAN button. Replaces the
    // long-dormant grey "NYI" label with a real dropdown that drives the
    // PanadapterStack (add slice on active pan, pick layout template, or
    // float the active pan into its own top-level window). The actual
    // PanadapterStack instance is wired by Task 12; until then,
    // showPanMenu's m_panStack-dependent actions no-op safely.
    auto* panBtn = new QPushButton(QStringLiteral("+PAN"), barWidget);
    panBtn->setFlat(true);
    panBtn->setCursor(Qt::PointingHandCursor);
    panBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; font-weight: bold; font-size: 11px;"
        " border: 1px solid %2; border-radius: 3px; padding: 2px 7px;"
        " background: %3; }"
        "QPushButton:hover { background: %4; }")
        .arg(Style::kAccent, Style::kBorder, Style::kButtonBg,
             Style::kButtonAltHover));
    panBtn->setToolTip(QStringLiteral("Add slice / change layout / float pan"));
    connect(panBtn, &QPushButton::clicked, this, &MainWindow::showPanMenu);
    hbox->addWidget(panBtn);

    // Phase 3F Sub-Epic D Task 11: per-chain (ADC) BPF state indicators
    // in the bottom status bar. Two stacked labels per chain ("CH N"
    // header + reasonText body), wired to AlexController::bpfStateChanged
    // below so the body text + colour reflect the live per-ADC BPF
    // state. CH 0 is always shown; CH 1 is shown only when the
    // connected radio's BoardCapabilities reports rxFilterChainCount >= 2
    // (gated in the currentRadioChanged handler below).
    auto makeChainIndicator = [&](int adc) -> QWidget* {
        auto* w  = new QWidget(barWidget);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        auto* topLbl = new QLabel(QStringLiteral("CH %1").arg(adc), w);
        topLbl->setStyleSheet(
            QStringLiteral("color: %1; font-size: 10px; font-weight: bold;")
                .arg(Style::kTextScale));
        vl->addWidget(topLbl);

        auto* botLbl = new QLabel(QStringLiteral("idle"), w);
        botLbl->setObjectName(QStringLiteral("chainIndicator%1").arg(adc));
        botLbl->setStyleSheet(
            QStringLiteral("color: %1; font-size: 9px; font-weight: bold;")
                .arg(Style::kTextInactive));
        vl->addWidget(botLbl);

        return w;
    };

    hbox->addWidget(makeChainIndicator(0));
    auto* chain1Widget = makeChainIndicator(1);
    chain1Widget->setVisible(false);  // shown only on 2-ADC SKUs
    hbox->addWidget(chain1Widget);
    m_chain1IndicatorWidget = chain1Widget;

    // Panel toggle (☰) — wired to QSplitter right pane visibility
    auto* panelToggleLabel = new QLabel(QStringLiteral("☰"), barWidget);
    panelToggleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }"));
    panelToggleLabel->setToolTip(QStringLiteral("Toggle container panel"));
    panelToggleLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(panelToggleLabel);

    // Wire ☰ click: toggle QSplitter right pane (widget index 1) visibility.
    // When hiding: save sizes so we can restore them. When showing: restore.
    connect(panelToggleLabel, &QLabel::linkActivated, this, [](const QString&){});
    // QLabel doesn't emit click directly — install event filter via lambda via
    // a helper QObject. Use mousePressEvent via event filter on the label.
    panelToggleLabel->installEventFilter(this);
    // We need to store the label pointer to recognise it in eventFilter.
    // Use a property to mark it.
    panelToggleLabel->setProperty("isPanelToggle", true);

    // TNF toggle
    m_tnfLabel = new QLabel(QStringLiteral("TNF"), barWidget);
    m_tnfLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    m_tnfLabel->setToolTip(QStringLiteral("Tracking Notch Filter (NYI)"));
    m_tnfLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(m_tnfLabel);

    // CWX
    auto* cwxLabel = new QLabel(QStringLiteral("CWX"), barWidget);
    cwxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    cwxLabel->setToolTip(QStringLiteral("CW Keyer (NYI)"));
    cwxLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(cwxLabel);

    // DVK
    auto* dvkLabel = new QLabel(QStringLiteral("DVK"), barWidget);
    dvkLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    dvkLabel->setToolTip(QStringLiteral("Digital Voice Keyer (NYI)"));
    dvkLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(dvkLabel);

    // FDX
    auto* fdxLabel = new QLabel(QStringLiteral("FDX"), barWidget);
    fdxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    fdxLabel->setToolTip(QStringLiteral("Full Duplex (NYI)"));
    fdxLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(fdxLabel);

    hbox->addWidget(makeSep());

    // Radio info: stacked model + firmware labels
    auto* radioInfoWidget = new QWidget(barWidget);
    QVBoxLayout* radioVbox = new QVBoxLayout(radioInfoWidget);
    radioVbox->setContentsMargins(0, 0, 0, 0);
    radioVbox->setSpacing(0);

    m_radioModelLabel = new QLabel(QStringLiteral("—"), radioInfoWidget);
    m_radioModelLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 11px; }"));
    radioVbox->addWidget(m_radioModelLabel);

    m_radioFwLabel = new QLabel(QStringLiteral("—"), radioInfoWidget);
    m_radioFwLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-size: 11px; }"));
    radioVbox->addWidget(m_radioFwLabel);

    // Keep m_connStatusLabel pointing at model label (legacy compat)
    m_connStatusLabel = m_radioModelLabel;

    hbox->addWidget(radioInfoWidget);

    // ── Phase 3Q Sub-PR-6 (F.1): RxDashboard ────────────────────────────────
    // Replaces the Phase 3Q-7 verbose connection-info strip (those fields now
    // live in the segment tooltip / NetworkDiagnosticsDialog).
    // Bound to slice(0); when disconnected the badges show placeholder "—"
    // until the slice receives live values from the radio.
    hbox->addWidget(makeSep());
    m_rxDashboard = new RxDashboard(barWidget);
    const auto& slices = m_radioModel->slices();
    if (!slices.isEmpty()) {
        m_rxDashboard->bindSlice(slices.at(0));
    }
    hbox->addWidget(m_rxDashboard);

    // ── Phase 3M-4 Task 10: PSA bottom-banner pair (FB + PS) ──────────────────
    // Source-first port of Thetis ucInfoBar.cs:820-1098 [v2.10.3.13].
    // The widget auto-wires to RadioModel's PureSignal coordinator and
    // MoxController on construction; click signals route back to
    // PureSignal::setInvertRedBlue / setHideFeedback below.
    //
    // Phase 3M-4 bench-fix: visibility is gated on
    //   caps.hasPureSignal && pureSignal->isAutoCalEnabled()
    // (NereusSDR-specific UX: hide the banner unless the user has
    // explicitly armed PS-A; reduces clutter for non-PS workflows on
    // PS-capable boards).  updatePsaIndicatorVisibility() centralises the
    // condition; called from autoCalEnabledChanged + connection-state +
    // pureSignalCoordinatorReady (late-bind seam, Task 13).
    m_psaIndicator = new PsaIndicatorWidget(m_radioModel, barWidget);
    m_psaIndicator->setVisible(false);
    auto wirePsaCoordinator = [this](PureSignal* ps) {
        if (!ps) return;
        connect(m_psaIndicator,
                &PsaIndicatorWidget::invertRedBlueRequested,
                this, [ps]() {
                    ps->setInvertRedBlue(!ps->invertRedBlue());
                });
        connect(m_psaIndicator,
                &PsaIndicatorWidget::hideFeedbackToggleRequested,
                this, [ps]() {
                    ps->setHideFeedback(!ps->hideFeedback());
                });
        connect(ps, &PureSignal::autoCalEnabledChanged,
                this, &MainWindow::updatePsaIndicatorVisibility);
    };
    wirePsaCoordinator(m_radioModel->pureSignal());
    connect(m_radioModel, &RadioModel::pureSignalCoordinatorReady,
            this, [this, wirePsaCoordinator](PureSignal* ps) {
                wirePsaCoordinator(ps);
                updatePsaIndicatorVisibility();
            });
    hbox->addWidget(m_psaIndicator);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Center section: STATION — radio-name anchor (Sub-PR-7 G.1) ───────────
    // The old cyan "STATION: NereusSDR" box is replaced by a StationBlock that
    // shows the connected radio's name. Click → opens ConnectionPanel. Right-
    // click → Disconnect / Edit radio… / Forget radio. Disconnected appearance:
    // dashed-red border + italic "Click to connect" placeholder.
    // The StationCallsign AppSettings key is preserved on disk for a potential
    // future operator-callsign surface; it is no longer shown in status chrome.
    m_stationBlock = new StationBlock(barWidget);
    connect(m_stationBlock, &StationBlock::clicked,
            this, &MainWindow::showConnectionPanel);
    connect(m_stationBlock, &StationBlock::contextMenuRequested,
            this, &MainWindow::showStationContextMenu);
    // Update the block's name on connection state changes.
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            [this](const NereusSDR::RadioInfo& info) {
        const bool connected =
            (m_radioModel->connectionState() == ConnectionState::Connected);
        m_stationBlock->setRadioName(connected ? info.name : QString());
    });
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState s) {
        if (s != ConnectionState::Connected) {
            m_stationBlock->setRadioName(QString());
        }
    });

    // ── ADC Overload alarm: lives in the right-side strip ──────────────────
    // Earlier revisions of the Phase 3Q chrome work parked the alarm
    // between the dashboard and STATION block. That violated layout-
    // stability rule §278.4 ("STATION sits between two flex:1 spacers.
    // Activity in the middle or right sections never moves it.")
    // because the label's text width grew when overload fired and
    // pushed STATION sideways. The alarm is now an AdcOverloadBadge
    // built further down with the other right-side status badges,
    // between PA OK and TX, where its size changes consume the right-
    // side strip's stretch space rather than the STATION anchor.

    hbox->addWidget(m_stationBlock);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Right section: indicators ────────────────────────────────────────────

    // Helper lambda: create a stacked indicator pair (top label + bottom label)
    // Returns the widget; sets topLbl/botLbl via out-params for wiring.
    auto makeIndicator = [&](const QString& top, const QString& bottom,
                              QLabel** outTop = nullptr, QLabel** outBot = nullptr) -> QWidget* {
        QWidget* w = new QWidget(barWidget);
        w->setMinimumWidth(60);
        QVBoxLayout* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        auto* topLbl = new QLabel(top, w);
        topLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 11px; }"));
        auto* botLbl = new QLabel(bottom, w);
        botLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #404858; font-size: 11px; }"));
        vl->addWidget(topLbl);
        vl->addWidget(botLbl);
        if (outTop) { *outTop = topLbl; }
        if (outBot) { *outBot = botLbl; }
        return w;
    };

    // CAT Serial — NYI until Phase 3K; kept as static indicator, no live signal
    m_catIndicator = makeIndicator(QStringLiteral("CAT"), QStringLiteral("Off"));
    hbox->addWidget(m_catIndicator);
    m_catSep = makeSep();
    hbox->addWidget(m_catSep);

    // TCI — Phase 23: capture bottom label for updateTciIndicator() + install
    // event filter for click-to-Setup navigation.
    m_tciIndicator = makeIndicator(QStringLiteral("TCI"), QStringLiteral("Off"),
                                   nullptr, &m_tciIndicatorBotLabel);
    m_tciIndicator->installEventFilter(this);
    hbox->addWidget(m_tciIndicator);
    m_tciSep = makeSep();
    hbox->addWidget(m_tciSep);

    // ── PA telemetry tile (volts + temperature, vertically stacked) ───────
    // Earlier revisions showed only a single "PSU" widget driven by the
    // supply_volts (AIN6) channel.  Source-first audit against Thetis
    // [v2.10.3.13] proved that channel is never displayed in Thetis —
    // computeHermesDCVoltage() exists but has zero callers, and the only
    // voltage status indicator (toolStripStatusLabel_Volts) reads
    // _MKIIPAVolts which is convertToVolts(getUserADC0()) — i.e. the PA
    // drain voltage on AIN3.  On a G2 / 8000D / 7000DLE the PA drain IS
    // the supply voltage minus a small drop, so this single number
    // covers what the user wants to know.
    //
    // The HL2 fork (mi0bot) extends this slot for HermesLite by reusing
    // the volts label to show FPGA on-die temperature (HL2 has no PA
    // volts ADC, but does carry a temperature ADC value in the C&C
    // exciter_power AIN5 field).  See mi0bot console.cs:26758-26762
    // [v2.10.3.13-beta2 @c26a8a4]:
    //   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
    //   {
    //       toolStripStatusLabel_Volts.Text = String.Format("{0:#0.0}C", _MKIIHL2Temp);
    //       ...
    //   }
    //
    // NereusSDR splits this into a 2-row stacked tile so MKII boards keep
    // their PA-V row, HL2 gets a dedicated PA-T row, and a future
    // PureSignal-feedback board (Phase 3M-4) can surface temp on
    // non-HL2 hardware without crowding the volts slot.  Both rows hide
    // independently based on data source; the container hides when
    // neither has data so non-MKII non-HL2 boards (Atlas, plain Hermes,
    // Angelia, Orion) get no tile.
    m_paStackWidget = new QWidget(barWidget);
    {
        QVBoxLayout* paStack = new QVBoxLayout(m_paStackWidget);
        paStack->setContentsMargins(0, 0, 0, 0);
        paStack->setSpacing(0);

        m_paVoltLabel = new MetricLabel(QStringLiteral("PA"),
                                        QStringLiteral("—"), m_paStackWidget);
        m_paVoltLabel->setVisible(false);
        paStack->addWidget(m_paVoltLabel);

        m_paTempLabel = new MetricLabel(QStringLiteral("PA T"),
                                        QStringLiteral("—"), m_paStackWidget);
        m_paTempLabel->setVisible(false);
        m_paTempLabel->setCursor(Qt::PointingHandCursor);
        m_paTempLabel->setToolTip(tr("Click to toggle °C / °F"));
        // Click anywhere on the PA-T row toggles the persisted unit
        // preference; PaTempUnitNotifier::unitChanged is wired below to
        // re-format every PA-temperature surface app-wide.
        m_paTempLabel->installEventFilter(this);
        m_paTempLabel->setProperty("isPaTempToggle", true);
        // MetricLabel has child QLabels that would otherwise eat the
        // mouse press before it reaches our event filter on the parent.
        // WA_TransparentForMouseEvents on each child makes the click
        // pass through to m_paTempLabel itself.
        for (QLabel* child : m_paTempLabel->findChildren<QLabel*>()) {
            child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
        paStack->addWidget(m_paTempLabel);
    }
    m_paStackWidget->setVisible(false);
    hbox->addWidget(m_paStackWidget);
    m_paVoltLabelSep = makeSep();
    m_paVoltLabelSep->setVisible(false);
    hbox->addWidget(m_paVoltLabelSep);

    // Phase 3P-II Task 21: TGXL presence chip.
    // Hidden until TunerModel::presenceChanged fires true; text reflects
    // operate/bypass/standby state via stateChanged.
    m_tgxlChip = new QLabel(QStringLiteral("TGXL"), barWidget);
    m_tgxlChip->setStyleSheet(QStringLiteral(
        "QLabel { background:#1a3a5a; border:1px solid #205070; "
        "padding:1px 8px; border-radius:3px; color:#88e0ff; }"));
    m_tgxlChip->setVisible(false);
    hbox->addWidget(m_tgxlChip);

    connect(m_radioModel->tunerModel(), &TunerModel::presenceChanged,
            m_tgxlChip, &QWidget::setVisible);
    connect(m_radioModel->tunerModel(), &TunerModel::stateChanged,
            this, [this]() {
        TunerModel* t = m_radioModel->tunerModel();
        QString s = t->isOperate()
                    ? (t->isBypass() ? QStringLiteral("BYPS")
                                     : QStringLiteral("OPER"))
                    : QStringLiteral("SBY");
        m_tgxlChip->setText(QStringLiteral("TGXL ") + s);
    });

    // Helper: reflect current row visibility back onto the container, so
    // when both rows are hidden the trailing separator collapses too.
    auto refreshPaStackVisibility = [this]() {
        const bool anyRow = (m_paVoltLabel && m_paVoltLabel->isVisible()) ||
                            (m_paTempLabel && m_paTempLabel->isVisible());
        const bool wasShown = m_paStackWidget->isVisible();
        m_paStackWidget->setVisible(anyRow);
        if (m_paVoltLabelSep) { m_paVoltLabelSep->setVisible(anyRow); }
        if (wasShown != anyRow) {
            // Tile width changed — drops may need to unwind / re-apply.
            // force=true: budget hasn't moved but content width has.
            reapplyRightStripDropPriority(/*force=*/true);
        }
    };

    // Wire voltage signals: re-bind on every new connection, reset on disconnect.
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this, refreshPaStackVisibility](ConnectionState s) {
        if (s != ConnectionState::Connected) {
            m_paVoltLabel->setVisible(false);
            m_paTempLabel->setVisible(false);
            refreshPaStackVisibility();
        }
        if (auto* conn = m_radioModel->connection()) {
            // conn is a new object on each reconnect — no deduplication needed.
            // Qt::UniqueConnection is not supported for lambda connects anyway.
            //
            // 2026-05-25 KG4VCF G2E bench finding: ANAN-G2E (HermesC10)
            // firmware leaves user_adc0 (AIN3 / status bytes 53-54) dark.
            // Bench reading was 0.1 V against an actual 13.4 V supply.
            // Route supply_volts (AIN6 / bytes 45-46) to the tile on G2E
            // and rename "PA" to "PSU".  Other MKII boards keep the
            // existing user_adc0 path -- on those SKUs user_adc0 IS the
            // PA drain sense, which is what the "PA" label means.
            //
            // Implementation note: we bind BOTH signals unconditionally
            // and gate inside each slot on the CURRENT model.  The outer
            // lambda fires on every connectionStateChanged transition
            // (Connecting / Probing / Connected), and at Connecting time
            // hardwareProfile.model may not yet be set to ANAN_G2E -- so
            // a branch-at-bind-time approach picked the wrong slot and
            // the tile stayed dark.  Gating inside the slot reads the
            // model at each signal emission, when it is guaranteed to be
            // set (status frames only arrive after the Connected handler
            // has populated the profile).
            connect(conn, &RadioConnection::userAdc0Changed, this,
                    [this, refreshPaStackVisibility](float v) {
                const auto model = m_radioModel->hardwareProfile().model;
                if (model == HPSDRModel::ANAN_G2E) {
                    return;  // G2E uses supply_volts; ignore user_adc0.
                }
                // Task 3.6: ANAN-8000DLE user preference gate.
                // For ANAN-8000D radios, consult the "Show volts/amps in title
                // bar" AppSettings key (default true). For other MKII-class
                // boards (7000DLE, AnvelinaPro3) the gate is always open —
                // those boards don't have the per-SKU preference checkbox.
                const bool is8000D = (model == HPSDRModel::ANAN8000D);
                const bool showVolts = !is8000D ||
                    AppSettings::instance().value(
                        QStringLiteral("HardwareAnan8000DleShowVoltsAmps"),
                        QStringLiteral("True")).toString() == QStringLiteral("True");
                if (!showVolts) { return; }
                m_paVoltLabel->setLabel(QStringLiteral("PA"));
                m_paVoltLabel->setValue(QString::asprintf("%.1fV", static_cast<double>(v)));
                m_paVoltLabel->setVisible(true);
                refreshPaStackVisibility();
                qInfo() << "PA tile updated via userAdc0:" << v << "V";
            });
            connect(conn, &RadioConnection::supplyVoltsChanged, this,
                    [this, refreshPaStackVisibility](float v) {
                const auto model = m_radioModel->hardwareProfile().model;
                if (model != HPSDRModel::ANAN_G2E) {
                    return;  // Non-G2E uses user_adc0 path.
                }
                m_paVoltLabel->setLabel(QStringLiteral("PSU"));
                m_paVoltLabel->setValue(QString::asprintf("%.1fV", static_cast<double>(v)));
                m_paVoltLabel->setVisible(true);
                refreshPaStackVisibility();
                qInfo() << "PSU tile updated via supplyVolts:" << v << "V";
            });
        }
    });

    // PA temperature row — driven by RadioStatus::paTemperatureChanged
    // (HL2 publishes via the handlePaTelemetry HL2 branch; future boards
    // may publish via the same RadioStatus signal).  The label
    // formatting respects PaTempUnitNotifier::currentUnit() so a
    // °C / °F toggle reformats live.
    connect(&m_radioModel->radioStatus(), &RadioStatus::paTemperatureChanged,
            this, [this, refreshPaStackVisibility](double celsius) {
        m_paTempLabel->setValue(PaTempUnitNotifier::format(celsius));
        m_paTempLabel->setVisible(true);
        refreshPaStackVisibility();
    });

    // Live re-format on °C / °F toggle without waiting for the next
    // telemetry sample.  Reads the cached °C value out of RadioStatus.
    connect(&PaTempUnitNotifier::instance(),
            &PaTempUnitNotifier::unitChanged, this,
            [this](PaTempUnit /*unit*/) {
        const double cachedC = m_radioModel->radioStatus().paTemperatureCelsius();
        m_paTempLabel->setValue(PaTempUnitNotifier::format(cachedC));
    });

    // ── sub-PR-8: CPU MetricLabel ─────────────────────────────────────────
    // Replaces the old stacked "CPU: —" / "Mem: —" makeIndicator pair.
    m_cpuMetric = new MetricLabel(QStringLiteral("CPU"),
                                  QStringLiteral("—"), barWidget);
    hbox->addWidget(m_cpuMetric);
    m_cpuMetricSep = makeSep();
    hbox->addWidget(m_cpuMetricSep);

    // ── Phase 3M-0 Task 14: TX Inhibit indicator ─────────────────────────
    // Red "TX INHIBIT" pill — hidden by default; shown when
    // TxInhibitMonitor::inhibited() asserts. Signal wiring to the monitor
    // lands in Task 17 (final integration pass).
    m_txInhibitLabel = new QLabel(QStringLiteral("TX INHIBIT"), barWidget);
    m_txInhibitLabel->setObjectName(QStringLiteral("txInhibitLabel"));
    m_txInhibitLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ff6060; font-weight: bold; font-size: 11px;"
        "         padding: 2px 6px; border: 1px solid #ff6060; border-radius: 3px; }"));
    m_txInhibitLabel->setToolTip(tr("External TX Inhibit asserted — TX is blocked"));
    m_txInhibitLabel->setVisible(false);  // hidden until inhibit asserts
    hbox->addWidget(m_txInhibitLabel);

    // ── sub-PR-8: PA Status StatusBadge ──────────────────────────────────
    // Variant::On (green ✓ PA OK) / Variant::Tx (red ✓ PA FAULT).
    // Driven by RadioModel::paTripped(); setPaTripped() flips the variant.
    // Signal wiring lands in Task 17 (same as the original QLabel).
    m_paStatusBadge = new StatusBadge(barWidget);
    m_paStatusBadge->setObjectName(QStringLiteral("paStatusBadge"));
    // SVG-backed icon — earlier revisions used the U+2713 CHECK MARK
    // glyph, which renders inconsistently across the SF Mono / Menlo /
    // monospace fallback chain (boxed or kerned wrong on platforms
    // without SF Mono installed). The SVG is rendered at 14 logical
    // px and tinted with the variant's foreground color.
    m_paStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-check.svg"));
    m_paStatusBadge->setLabel(QStringLiteral("PA"));
    m_paStatusBadge->setVariant(StatusBadge::Variant::On);
    m_paStatusBadge->setToolTip(tr("PA Status — OK"));
    hbox->addWidget(m_paStatusBadge);
    m_paStatusBadgeSep = makeSep();
    hbox->addWidget(m_paStatusBadgeSep);

    // ── ADC overload alarm — stacked badge between PA OK and TX ───────────
    // Hidden by default; shown when StepAttenuatorController emits an
    // overload event, hidden again 2 s after the latest event by the
    // timer below. The trailing separator is captured + toggled with
    // the badge so the strip closes seamlessly when the alarm clears
    // (otherwise we'd leave a dangling "··" run).
    //
    // Source-first port of Thetis pollOverloadSyncSeqErr + ucInfoBar.Warning
    // [@501e3f5]:
    //   console.cs:21323        adc_names[] = { "ADC0", "ADC1", "ADC2" }
    //   console.cs:21359-21389  per-ADC level counter; level>0 → warn,
    //                            any level>3 → red_warning
    //   ucInfoBar.cs:911-933    Warning(msg, red_warning, show_duration):
    //                            ForeColor = red ? Red : Yellow;
    //                            Visible=true; _warningTimer.Start()
    m_adcOvlBadge = new AdcOverloadBadge(barWidget);
    m_adcOvlBadge->setObjectName(QStringLiteral("adcOvlBadge"));
    m_adcOvlBadge->setVisible(false);
    hbox->addWidget(m_adcOvlBadge);
    m_adcOvlSep = makeSep();
    m_adcOvlSep->setVisible(false);
    hbox->addWidget(m_adcOvlSep);

    // Auto-hide timer mirrors Thetis ucInfoBar._warningTimer — single-shot
    // 2000 ms, restarts on each overload event so a single hit keeps the
    // alarm visible for the full 2 s even after the per-ADC level decays.
    // Source: ucInfoBar.cs:927-932 + console.cs:21388 show_duration=2000
    // [@501e3f5].
    m_adcOvlHideTimer = new QTimer(this);
    m_adcOvlHideTimer->setSingleShot(true);
    m_adcOvlHideTimer->setInterval(2000);
    connect(m_adcOvlHideTimer, &QTimer::timeout, this, [this]() {
        if (m_adcOvlBadge) { m_adcOvlBadge->setVisible(false); }
        if (m_adcOvlSep)   { m_adcOvlSep->setVisible(false); }
        // Required width of the strip just shrank — recompute drops.
        // force=true: budget hasn't moved but content width has.
        reapplyRightStripDropPriority(/*force=*/true);
    });

    connect(m_stepAttController, &StepAttenuatorController::overloadStatusChanged,
            this, [this](int /*adc*/, OverloadLevel /*level*/) {
        // Thetis adc_names table — console.cs:21323 [@501e3f5]
        static const char* const kAdcNames[3] = { "ADC0", "ADC1", "ADC2" };

        // Build the alarm state: which ADCs are firing, plus highest
        // severity. Thetis console.cs:21359-21389 [@501e3f5] —
        // red_warning is any level > 3; our levelToSeverity() maps that
        // to OverloadLevel::Red.
        bool anyRed = false;
        QString shownAdcs;
        QString tip;
        for (int i = 0; i < 3; ++i) {
            const OverloadLevel lvl = m_stepAttController->overloadLevel(i);
            if (lvl == OverloadLevel::None) { continue; }
            if (lvl == OverloadLevel::Red) { anyRed = true; }
            if (!shownAdcs.isEmpty()) { shownAdcs += QStringLiteral("/"); }
            shownAdcs += QString::number(i);
            if (!tip.isEmpty()) { tip += QStringLiteral("\n"); }
            tip += QStringLiteral("%1: overload").arg(
                QString::fromLatin1(kAdcNames[i]));
        }

        if (shownAdcs.isEmpty()) {
            // No ADC currently above level 0 — let the 2 s auto-hide
            // timer expire so a just-cleared overload stays visible
            // for the remainder of its window. Matches Thetis.
            return;
        }

        m_adcOvlBadge->setAdcs(shownAdcs);
        // ucInfoBar.cs:928 [@501e3f5] — red_warning ? Red : Yellow.
        m_adcOvlBadge->setVariant(anyRed ? AdcOverloadBadge::Variant::Tx
                                         : AdcOverloadBadge::Variant::Warn);
        m_adcOvlBadge->setToolTip(tip);
        m_adcOvlBadge->setVisible(true);
        if (m_adcOvlSep) { m_adcOvlSep->setVisible(true); }

        // Required width of the strip just grew — drop something else
        // if the new total exceeds budget.
        // force=true: budget hasn't moved but content width has.
        reapplyRightStripDropPriority(/*force=*/true);

        // Restart auto-hide — Thetis: _warningTimer.Stop(); .Start();
        // (ucInfoBar.cs:927+932 [@501e3f5]).
        m_adcOvlHideTimer->start();
    });

    // ── sub-PR-8: Canonical TX StatusBadge ───────────────────────────────
    // Solid red (Variant::Tx) when MoxController emits moxStateChanged(true).
    // Dim (Variant::Off) at rest. No flash per design spec.
    m_txStatusBadge = new StatusBadge(barWidget);
    m_txStatusBadge->setObjectName(QStringLiteral("txStatusBadge"));
    // SVG-backed icon — see PA badge note above for rationale. The dot
    // shape matches the U+25CF BLACK CIRCLE glyph it replaces.
    m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    m_txStatusBadge->setLabel(QStringLiteral("TX"));
    m_txStatusBadge->setVariant(StatusBadge::Variant::Off);
    m_txStatusBadge->setToolTip(tr("Receive (MOX off)"));
    hbox->addWidget(m_txStatusBadge);
    hbox->addWidget(makeSep());

    // Wire TX badge to MoxController. MoxController lives on m_radioModel;
    // both are created before buildStatusBar() runs.
    if (MoxController* mox = m_radioModel->moxController()) {
        // Qt::UniqueConnection is not supported for lambda connects — this
        // connect runs once at construction so no deduplication is needed.
        connect(mox, &MoxController::moxStateChanged, this, [this](bool tx) {
            m_txStatusBadge->setVariant(tx ? StatusBadge::Variant::Tx
                                           : StatusBadge::Variant::Off);
            m_txStatusBadge->setToolTip(tx ? tr("Transmitting (MOX engaged)")
                                           : tr("Receive (MOX off)"));
        });
    }

    // ── OverflowChip — surfaces drop-list contents when the strip is tight
    // Sits just before the clock so the "…" appears at the right end of
    // the strip whenever ≥ 1 right-strip item has been dropped to fit.
    // Hidden when the drop list is empty.
    m_overflowChip = new OverflowChip(barWidget);
    hbox->addWidget(m_overflowChip);

    // Time display: stacked UTC + date / local
    // Top row: UTC time (hh:mm:ss UTC)
    // Bottom row: date + local time
    {
        m_timeWidget = new QWidget(barWidget);
        m_timeWidget->setMinimumWidth(130);
        QVBoxLayout* tvl = new QVBoxLayout(m_timeWidget);
        tvl->setContentsMargins(0, 0, 0, 0);
        tvl->setSpacing(0);

        m_utcTimeLabel = new QLabel(m_timeWidget);
        m_utcTimeLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 11px; }"));
        m_utcTimeLabel->setToolTip(QStringLiteral("UTC time"));
        tvl->addWidget(m_utcTimeLabel);

        auto* localDateLabel = new QLabel(m_timeWidget);
        localDateLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 11px; }"));
        localDateLabel->setToolTip(QStringLiteral("Local date/time"));
        tvl->addWidget(localDateLabel);

        hbox->addWidget(m_timeWidget);

        // Combined clock timer — 1s updates for UTC+date+local
        m_clockTimer = new QTimer(this);
        connect(m_clockTimer, &QTimer::timeout, this, [this, localDateLabel]() {
            QDateTime utcNow = QDateTime::currentDateTimeUtc();
            QDateTime localNow = QDateTime::currentDateTime();
            m_utcTimeLabel->setText(utcNow.toString(QStringLiteral("hh:mm:ss UTC")));
            localDateLabel->setText(
                localNow.toString(QStringLiteral("yyyy-MM-dd  hh:mm")));
        });
        // Fire once immediately so labels are populated before first tick
        QDateTime utcNow = QDateTime::currentDateTimeUtc();
        QDateTime localNow = QDateTime::currentDateTime();
        m_utcTimeLabel->setText(utcNow.toString(QStringLiteral("hh:mm:ss UTC")));
        localDateLabel->setText(localNow.toString(QStringLiteral("yyyy-MM-dd  hh:mm")));
        m_clockTimer->start(1000);
    }

    // ── CPU usage timer ──────────────────────────────────────────────────────
    // Two sources, user-toggleable via right-click on m_cpuMetric:
    //   System  (default) — host_processor_info / whole-machine CPU,
    //                       mirrors Thetis _total_cpu_usage PerformanceCounter.
    //   App     — getrusage(RUSAGE_SELF), this process only,
    //                       mirrors Thetis Common.ProcessCPUUsage.
    // 1 s tick rate matches Thetis cpu_meter_delay (console.cs:20102).
    // Smoothed value via 0.8/0.2 mix per Thetis console.cs:26224.
    // Restore persisted toggle (default System per Thetis default).
    // Wired on every supported platform — readSystemCpuPercent /
    // readProcessCpuPercent are cross-platform (macOS / Linux / Windows).
    m_cpuShowSystem = (AppSettings::instance()
                          .value(QStringLiteral("CpuShowSystem"),
                                 QStringLiteral("True"))
                          .toString() == QStringLiteral("True"));

    m_cpuMetric->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_cpuMetric, &QWidget::customContextMenuRequested,
            this, &MainWindow::onCpuMenuRequested);
    m_cpuMetric->setToolTip(
        tr("Right-click to switch between System and App CPU usage"));

    m_cpuTimer = new QTimer(this);
    connect(m_cpuTimer, &QTimer::timeout, this, [this]() {
        const double pct = m_cpuShowSystem ? readSystemCpuPercent()
                                           : readProcessCpuPercent();
        // Thetis smoothing: smoothed = smoothed*0.8 + new*0.2
        m_cpuSmoothedPct = m_cpuSmoothedPct * 0.8 + pct * 0.2;
        if (m_cpuMetric) {
            m_cpuMetric->setValue(QString::asprintf("%.0f%%",
                                                    m_cpuSmoothedPct));
        }
    });
    // Task 3.6: restore persisted rate (default 1 Hz = 1000 ms interval).
    {
        const int savedHz = AppSettings::instance().value(
            QStringLiteral("GeneralCpuMeterUpdateRateHz"), 1).toInt();
        const int clampedHz = qBound(1, savedHz, 30);
        m_cpuTimer->start(1000 / clampedHz);
    }

    // Add the full-width bar widget to the status bar.
    //
    // Deliberately addWidget rather than addPermanentWidget: the bar takes
    // the full width with stretch 1, so as a permanent widget it would
    // leave a showMessage no room to render and the notice would be lost
    // silently. Nothing calls showMessage any more for exactly that
    // reason; notices go through showToast() below. If you are about to
    // add a showMessage here, it will blank this entire bar. Use
    // showToast().
    sb->addWidget(barWidget, 1);
}

// ── Transient notices ────────────────────────────────────────────────────────
//
// Bench report 2026-07-30 (JJ, KG4VCF): pressing TUNE with PureSignal
// active replaced the whole bottom bar with a single line of text for
// six seconds. Root cause is not the message, it is the surface:
// QStatusBar::showMessage hides every non-permanent widget while a
// message is up, and buildStatusBar adds the entire bar as one such
// widget. So any notice cost the operator the CH pill, the PureSignal
// indicator, the radio name, CAT and TCI state, the PA and TX badges
// and the clock, all at once, mid-transmit.
//
// The notices themselves are worth keeping. They move here instead.
StatusToast* MainWindow::showToast(const QString& message,
                                   ToastSeverity severity,
                                   int timeoutMs)
{
    if (message.isEmpty()) { return nullptr; }

    // Drop dead entries first so a repeat check and the restack below
    // both see only live toasts.
    m_toasts.removeIf([](const QPointer<StatusToast>& t) { return t.isNull(); });

    // A repeat of something already on screen restarts its countdown
    // rather than stacking a second copy. Several of these fire from
    // signals that can repeat while the condition persists, and a column
    // of identical toasts is worse than the message it replaced.
    for (const QPointer<StatusToast>& existing : m_toasts) {
        if (existing && existing->message() == message) {
            existing->refresh(timeoutMs);
            return existing;
        }
    }

    auto* toast = new StatusToast(message, severity, timeoutMs, this);
    connect(toast, &QObject::destroyed, this, [this]() {
        m_toasts.removeIf([](const QPointer<StatusToast>& t) { return t.isNull(); });
        restackToasts();
    });
    m_toasts.append(toast);
    toast->show();
    restackToasts();
    return toast;
}

void MainWindow::restackToasts()
{
    // Bottom-right, newest nearest the bar, growing upward. Offset clears
    // the status bar itself so a toast never covers the thing it was
    // introduced to stop covering.
    const QRect geom = frameGeometry();
    const int rightEdge = geom.right() - 20;
    int bottom = geom.bottom() - 60;

    for (auto it = m_toasts.crbegin(); it != m_toasts.crend(); ++it) {
        StatusToast* toast = *it;
        if (!toast) { continue; }
        toast->move(rightEdge - toast->width(), bottom - toast->height());
        bottom -= toast->height() + 8;
    }
}

// ── Phase 3M-0 Task 14 / sub-PR-8: PA trip badge update ──────────────────────
// Called by Task 17 wiring when RadioModel::paTrippedChanged fires.
// Flips the StatusBadge variant (On = green ✓ PA OK; Tx = red ✓ PA FAULT)
// and updates the tooltip atomically.
void MainWindow::setPaTripped(bool tripped)
{
    if (!m_paStatusBadge) { return; }
    if (tripped) {
        m_paStatusBadge->setVariant(StatusBadge::Variant::Tx);
        m_paStatusBadge->setToolTip(tr("PA Status — FAULT (PA tripped, MOX dropped)"));
    } else {
        m_paStatusBadge->setVariant(StatusBadge::Variant::On);
        m_paStatusBadge->setToolTip(tr("PA Status — OK"));
    }
}

// ── Phase 3M-0 Task 14: TX Inhibit label visibility ─────────────────────────
// Called by Task 17 wiring when TxInhibitMonitor::txInhibitedChanged fires.
// Shows or hides the red "TX INHIBIT" pill in the status bar.
void MainWindow::setTxInhibited(bool inhibited)
{
    if (!m_txInhibitLabel) { return; }
    m_txInhibitLabel->setVisible(inhibited);
}

// ── Phase 23: TCI indicator update + Setup navigation ────────────────────────
//
// updateTciIndicator() — 4 states per design doc §8.4.
// Reads m_tciServerRunning / m_tciClientCount / m_tciHasTxClient (all updated
// by TciServer signal lambdas) and applies color + text + tooltip.
//
// Colors:
//   Off:       #404858 (dim grey)
//   On:        #6f6    (green)
//   On · N:    #6cf    (cyan)
//   On · N TX: #ec6    (orange)
void MainWindow::updateTciIndicator()
{
    if (!m_tciIndicatorBotLabel) { return; }

    QString text;
    QString color;
    QString tooltip;

#ifdef HAVE_WEBSOCKETS
    const quint16 port = m_tciServer ? m_tciServer->port() : 50001;
    const QString addr = QStringLiteral("127.0.0.1:%1").arg(port);
#else
    const QString addr = QStringLiteral("127.0.0.1:50001");
#endif

    if (!m_tciServerRunning) {
        text    = QStringLiteral("Off");
        color   = QStringLiteral("#404858");
        tooltip = QStringLiteral("TCI Server stopped. Click to open Setup.");
    } else if (m_tciClientCount == 0) {
        text    = QStringLiteral("On");
        color   = QStringLiteral("#6f6");
        tooltip = QStringLiteral("TCI Server listening on %1. No clients.").arg(addr);
    } else if (!m_tciHasTxClient) {
        text    = QStringLiteral("On · %1").arg(m_tciClientCount);
        color   = QStringLiteral("#6cf");
        tooltip = QStringLiteral("TCI Server listening on %1. %2 client%3 connected.")
                      .arg(addr)
                      .arg(m_tciClientCount)
                      .arg(m_tciClientCount == 1 ? QString{} : QStringLiteral("s"));
    } else {
        text    = QStringLiteral("On · %1 ▸TX").arg(m_tciClientCount);
        color   = QStringLiteral("#ec6");
        QString txPeer;
#ifdef HAVE_WEBSOCKETS
        if (m_tciServer) {
            txPeer = m_tciServer->activeTxClientPeer();
        }
#endif
        tooltip = QStringLiteral("TCI Server listening on %1. %2 client%3. %4 holds TX audio.")
                      .arg(addr)
                      .arg(m_tciClientCount)
                      .arg(m_tciClientCount == 1 ? QString{} : QStringLiteral("s"))
                      .arg(txPeer.isEmpty() ? QStringLiteral("A client") : txPeer);
    }

    m_tciIndicatorBotLabel->setText(text);
    m_tciIndicatorBotLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(color));
    if (m_tciIndicator) {
        m_tciIndicator->setToolTip(tooltip);
    }
}

// openTciSetupPage() — open Setup dialog at "TCI Server" page.
// Pattern-matched from the many other "open setup" sites in MainWindow.cpp
// (e.g. vfoWidget::openSetupRequested, m_overlayPanel::openSetupRequested).
void MainWindow::openTciSetupPage()
{
    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);
    dialog->selectPage(QStringLiteral("TCI Server"));
    dialog->show();
}

// openSetup(pageKey) -- Phase 3P-II Phase 4 Task 90
//
// Generic navigation entry point wired to applet right-click menus.
// Maps a well-known key string to a SetupDialog tree label and opens the
// dialog at that page. If the key is not recognised, logs a warning and
// opens the dialog at the default page (first leaf).
//
// Key -> tree label mapping (Option 1 per plan Task 90):
//   "pgxlAdvanced"  -> "PGXL Advanced"
//   "tgxlAdvanced"  -> "TGXL Advanced"
//   "pgxlInterlock" -> "PGXL Interlock"
//   "peripherals"   -> "Peripherals"
//
// Pattern matches openTciSetupPage(): fresh SetupDialog with WA_DeleteOnClose
// so geometry is not preserved across opens (consistent with all other Setup
// entry points in this file).
void MainWindow::openSetup(const QString& pageKey)
{
    static const QHash<QString, QString> kKeyToLabel = {
        {QStringLiteral("pgxlAdvanced"),  QStringLiteral("PGXL Advanced")},
        {QStringLiteral("tgxlAdvanced"),  QStringLiteral("TGXL Advanced")},
        {QStringLiteral("pgxlInterlock"), QStringLiteral("PGXL Interlock")},
        {QStringLiteral("peripherals"),   QStringLiteral("Peripherals")},
        // Phase 3P-III Task 14: RF-Kit setup page (Setup > CAT & Network > RF-Kit).
        {QStringLiteral("rfKit"),         QStringLiteral("RF-Kit")},
    };

    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);

    const QString label = kKeyToLabel.value(pageKey);
    if (label.isEmpty()) {
        qWarning("MainWindow::openSetup: unknown pageKey '%s' -- opening at default page",
                 qUtf8Printable(pageKey));
    } else {
        dialog->selectPage(label);
    }
    dialog->show();
    dialog->raise();
}

// Phase 3P-II Phase 4 Task 97: PGXL power cap soft-alert toast.
//
// Fires a 5-second QStatusBar toast when peak forward power exceeds the
// operator-configured PGXL cap.  De-bounced: one toast per exceedance event
// (re-armed when fwd drops back below the cap threshold so a subsequent
// exceedance fires a fresh toast).
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
//   Task 97 / design ss5.6.2 "TX power cap: soft alert only".
//
// Keys:
//   PGXL_PowerCapEnabled  -- "True"/"False", default "False"
//   PGXL_PowerCapW        -- int watts, default 1500
//
// Connected to RadioModel::ampMetersChanged in buildUI() near Task 43.
void MainWindow::onAmpMetersForPowerCap(float fwd, float /*swr*/)
{
    const bool enabled = AppSettings::instance()
        .value(QStringLiteral("PGXL_PowerCapEnabled"), QStringLiteral("False"))
        .toString() == QStringLiteral("True");
    if (!enabled) {
        m_powerCapToastShown = false;   // keep re-arm state sane if feature toggled
        return;
    }

    const float capW = AppSettings::instance()
        .value(QStringLiteral("PGXL_PowerCapW"), 1500).toFloat();

    if (fwd <= capW) {
        m_powerCapToastShown = false;   // re-arm: fwd is back below cap
        return;
    }

    if (m_powerCapToastShown) { return; }   // de-bounce: already toasted this exceedance
    m_powerCapToastShown = true;

    const QString msg = QStringLiteral("PGXL power %1 W exceeds cap %2 W")
        .arg(static_cast<int>(fwd))
        .arg(static_cast<int>(capW));
    showToast(msg, ToastSeverity::Error, 5000);
    qCWarning(lcMeter) << msg;
}

// ── Phase 3P-II review fix C2: TX interlock warning/denial toasts ────────────
// Both slots display a 5-second notice so the operator knows why TX was
// warned or blocked.  The distinction: warning allows TX to proceed; denial
// means MOX was rejected.  Pattern mirrors onAmpMetersForPowerCap above
// (showToast + qCWarning(lcMeter)).
//
// These are the strongest argument for not using QStatusBar::showMessage
// here: both fire during transmit, and blanking the bottom bar would take
// the PA and TX badges away at the exact moment they matter.
void MainWindow::onTxInterlockWarning(const QString& reason)
{
    const QString msg = QString("TX interlock warning: %1").arg(reason);
    showToast(msg, ToastSeverity::Warning, 5000);
    qCWarning(lcMeter) << msg;
}

void MainWindow::onTxInterlockDenial(const QString& reason)
{
    const QString msg = QString("TX interlock blocked: %1").arg(reason);
    showToast(msg, ToastSeverity::Error, 5000);
    qCWarning(lcMeter) << msg;
}

// ── Task 3.6: CPU meter rate ─────────────────────────────────────────────────
// Live-applies the CPU meter update interval from GeneralOptionsPage spinbox.
// Restarts m_cpuTimer with the new period. hz is clamped to [1, 30] so a
// zero or negative value from a misconfigured spinbox cannot stop the timer.
void MainWindow::setCpuTimerIntervalHz(int hz)
{
    if (!m_cpuTimer) { return; }
    const int clamped = qBound(1, hz, 30);
    m_cpuTimer->setInterval(1000 / clamped);
}

// ── Task 3.6: ANAN-8000DLE volts/amps visibility ────────────────────────────
// Called when the "Show volts/amps in title bar" checkbox on Hardware →
// Radio Info is toggled.  Only has visible effect for ANAN-8000D radios
// (m_paVoltLabel is already hidden for non-MKII boards by the hardware gate
// in buildStatusBar(); this gives the user an additional opt-out).
void MainWindow::setVoltsAmpsVisible(bool visible)
{
    if (!m_paVoltLabel) { return; }
    // Only change visibility when the widget is already populated (i.e. a
    // MKII-class radio is connected and has sent at least one userAdc0Changed
    // signal). If not yet shown, the user preference is stored in AppSettings
    // and consulted by the userAdc0Changed lambda in buildStatusBar().
    if (!visible && m_paVoltLabel->isVisible()) {
        m_paVoltLabel->setVisible(false);
        // Container + trailing separator only collapse if the PA-T row
        // is also hidden. HL2's temp row keeps the tile alive even when
        // the user opts out of volts display on a (hypothetical) board
        // that surfaces both rows; today no single board does, but the
        // logic is symmetric with refreshPaStackVisibility() in
        // buildStatusBar().
        const bool anyRow = m_paTempLabel && m_paTempLabel->isVisible();
        if (m_paStackWidget) { m_paStackWidget->setVisible(anyRow); }
        if (m_paVoltLabelSep) { m_paVoltLabelSep->setVisible(anyRow); }
        reapplyRightStripDropPriority(/*force=*/true);
    }
    // Note: toggling back to visible=true does not force-show the widget;
    // the next userAdc0Changed will re-show it. This avoids showing a stale
    // "—" value before the first ADC reading arrives.
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task 3) — wireSetupDialog
//
// Centralized helper called from every SetupDialog construction site.  Wires
// the dialog's cfcDialogRequested signal (forwarded from CfcSetupPage's
// [Configure CFC bands…] button) to TxApplet::requestOpenCfcDialog so the
// modeless TxCfcDialog instance owned by the TxApplet is reused.
//
// Pre-condition: m_txApplet is set (TxApplet is created during early UI
// build-out, well before any of the user-triggered SetupDialog opens).
// ---------------------------------------------------------------------------
void MainWindow::wireSetupDialog(SetupDialog* dialog)
{
    if (!dialog) { return; }
    if (m_txApplet) {
        connect(dialog, &SetupDialog::cfcDialogRequested,
                m_txApplet, &TxApplet::requestOpenCfcDialog);
    }
    // Phase 3P-II Phase 4 Task 95: propagate TGXL antenna label edits from
    // Setup -> Network -> TGXL Advanced -> Antenna Labels to the TunerApplet
    // antenna buttons so they update live without restarting.
    if (m_tunerApplet) {
        connect(dialog, &SetupDialog::tgxlAntennaLabelChanged,
                m_tunerApplet, &TunerApplet::onAntennaLabelChanged);
    }
    // Task 3.6: CPU meter rate live-apply.
    connect(dialog, &SetupDialog::cpuMeterRateChanged,
            this,   &MainWindow::setCpuTimerIntervalHz);
    // Task 3.6: ANAN-8000DLE volts/amps live-apply.
    connect(dialog, &SetupDialog::anan8000DleVoltsAmpsChanged,
            this,   &MainWindow::setVoltsAmpsVisible);

#ifdef HAVE_WEBSOCKETS
    // Phase 3J-1 review P2.4: live-wire Setup → Network → TCI Server enable
    // checkbox to the running TciServer.  Previously the checkbox only wrote
    // AppSettings; the server required a manual restart to pick up the change.
    // Now toggling the checkbox immediately starts or stops the server.
    //
    // Auto-connect (QueuedConnection for cross-dialog safety): when `on` is
    // true, start on the persisted port; when false, stop.  If the server is
    // already in the requested state the calls are no-ops (double-start
    // returns false; stop() on a non-running server returns immediately).
    if (m_tciServer) {
        connect(dialog, &SetupDialog::tciServerEnableToggled,
                this, [this](bool on, quint16 port) {
                    if (on) {
                        // Re-read bind address from AppSettings so the start
                        // honors whatever the operator last picked in the
                        // bind-interface dropdown.  CatTciServerPage persists
                        // TciServerBindAddress on every combo change.
                        auto& s = AppSettings::instance();
                        const QString bindStr = s.value(
                            QStringLiteral("TciServerBindAddress"),
                            QStringLiteral("127.0.0.1")).toString();
                        QHostAddress bindAddr;
                        if (!bindAddr.setAddress(bindStr)) {
                            bindAddr = QHostAddress(QHostAddress::LocalHost);
                        }
                        m_tciServer->start(bindAddr, port);
                    } else {
                        m_tciServer->stop();
                    }
                });
        // Phase 3J-1 closeout Item 1 (2026-05-12): live-restart on bind /
        // port change.  CatTciServerPage emits this whenever the operator
        // picks a different interface from the dropdown or edits the port
        // spinbox.  If the server is running, restart it in place; if
        // stopped, the new values are already in AppSettings for next
        // start.  Mirrors the enable-toggled pattern above.
        connect(dialog, &SetupDialog::tciServerBindOrPortChanged,
                this, [this](const QString& bindStr, quint16 port) {
                    if (!m_tciServer->isRunning()) {
                        return;
                    }
                    QHostAddress bindAddr;
                    if (!bindAddr.setAddress(bindStr)) {
                        bindAddr = QHostAddress(QHostAddress::LocalHost);
                    }
                    m_tciServer->stop();
                    m_tciServer->start(bindAddr, port);
                });
        // Phase 3J-1 closeout Item 2 (2026-05-12): "Show Log..." button.
        // The window is owned by MainWindow (lazy-constructed) so it
        // outlives this dialog closing -- WSJT-X sessions can take an
        // hour to settle and the operator wants the log window pinned.
        connect(dialog, &SetupDialog::tciShowLogRequested,
                this,   &MainWindow::showTciLogWindow);

        // Phase 3J-1 bench fix (2026-05-11): forward the TciServer reference
        // into the dialog so CatTciServerPage's Server group box title +
        // Status label update live as clients connect/disconnect and the
        // server starts/stops.  Mirrors Thetis Setup.cs:9491-9494
        // [v2.10.3.13] — TCIClientsConnectedChange updates `grpTCIServer.Text`.
        dialog->setTciServer(m_tciServer);
    }
#endif // HAVE_WEBSOCKETS
}

#ifdef HAVE_WEBSOCKETS
// Phase 3J-1 closeout Item 2 (2026-05-12): lazy-construct the TciLogWindow
// on first request, connect it to TciServer::messageLogged, and show it.
// Subsequent clicks just raise the existing window.  The window is owned
// by MainWindow so it survives Setup dialog close/reopen.
//
// Qt::QueuedConnection on the signal hookup ensures the TciServer's
// emit-side never blocks while the log view processes the entry --
// important during a busy WSJT-X session where ~10 frames/sec arrive
// from each direction.
void MainWindow::showTciLogWindow()
{
    if (!m_tciServer) {
        return;  // No-op in builds without HAVE_WEBSOCKETS or before init.
    }
    if (!m_tciLogWindow) {
        m_tciLogWindow = new TciLogWindow(this);
        connect(m_tciServer, &TciServer::messageLogged,
                m_tciLogWindow, &TciLogWindow::appendEntry,
                Qt::QueuedConnection);
    }
    m_tciLogWindow->show();
    m_tciLogWindow->raise();
    m_tciLogWindow->activateWindow();
}
#else
void MainWindow::showTciLogWindow() {}  // no-op in non-WebSocket builds
#endif // HAVE_WEBSOCKETS

void MainWindow::wireSliceToSpectrum()
{
    SliceModel* slice = m_radioModel->activeSlice();
    if (!slice || !activeSpectrumWidget()) {
        return;
    }

    // Set initial spectrum display. Phase 3G-12: preserve the user's
    // persisted zoom level if present. SpectrumWidget::loadSettings()
    // has already read "DisplayBandwidth" from AppSettings into
    // m_bandwidthHz by this point. If the loaded value is sensible
    // (between 10 kHz and the DDC sample rate), keep it; otherwise
    // fall back to the full-span default (768 kHz = sample rate).
    double freq = slice->frequency();
    const double loadedBw = activeSpectrumWidget()->bandwidth();
    const double initialBw = (loadedBw >= 10000.0 && loadedBw <= 768000.0)
                             ? loadedBw : 768000.0;
    activeSpectrumWidget()->setFrequencyRange(freq, initialBw);
    activeSpectrumWidget()->setDdcCenterFrequency(freq);
    activeSpectrumWidget()->setSampleRate(768000.0);
    activeSpectrumWidget()->setVfoFrequency(freq);
    activeSpectrumWidget()->setFilterOffset(slice->filterLow(), slice->filterHigh());
    activeSpectrumWidget()->setStepSize(slice->stepHz());

    // --- Create floating VFO flag widget (AetherSDR pattern) ---
    // Slice A's flag is built and wired by createSliceFlag, exactly like every
    // other slice's. This used to construct it by hand and then wire 67
    // connects to it, while createSliceFlag wired far fewer -- two paths, one
    // incomplete, which is why 23 flag controls were dead on B/C/D. One path
    // now, so the two cannot drift again.
    VfoWidget* vfo = createSliceFlag(slice, activeSpectrumWidget());
    if (!vfo) { return; }
    m_vfoWidget = vfo;

    // Mode-driven applet surfaces. These are SINGLE global widgets (one RADE
    // applet, one PhoneCw applet), so they follow the ACTIVE slice's mode and
    // stay here rather than moving into the per-flag path.
    //
    // Restored after the flag-path unification deleted the handler that
    // carried them alongside the flag's own setMode: switching to CW or FM
    // stopped changing the PhoneCw page, and RADE stopped revealing its
    // applet. The passband and TX-mode half of that handler now lives in
    // createSliceFlag, per pan.
    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        // Phase 3R L2: RADE applet shows for either RADE sideband, IN ADDITION
        // to PhoneCwApplet -- bench feedback showed PhoneCw hosts the mic gain
        // slider, which RADE TX still needs. Routed through the visibility
        // controller so the wrapper and the menu entry agree, and the user's
        // persisted preference survives the mode change.
        const bool isRade = (mode == DSPMode::RADE_U
                             || mode == DSPMode::RADE_L);
        if (m_appletVis) {
            m_appletVis->setAvailable(QStringLiteral("Rade"), isRade);
        }
        if (m_phoneCwApplet) {
            m_phoneCwApplet->setVisible(true);  // always visible
            switch (mode) {
                case DSPMode::CWL:
                case DSPMode::CWU:
                    m_phoneCwApplet->showPage(1);  // CW page
                    break;
                case DSPMode::FM:
                    m_phoneCwApplet->showPage(2);  // FM page
                    break;
                default:
                    m_phoneCwApplet->showPage(0);  // Phone page
                                                   // (incl. RADE_U / RADE_L)
                    break;
            }
        }
    });
    vfo->setSlice(slice);
    vfo->setFrequency(freq);
    vfo->setMode(slice->dspMode());
    vfo->setFilter(slice->filterLow(), slice->filterHigh());
    vfo->setAgcMode(slice->agcMode());
    vfo->setAfGain(slice->afGain());
    vfo->setRfGain(slice->rfGain());
    vfo->setRxAntenna(slice->rxAntenna());
    vfo->setTxAntenna(slice->txAntenna());
    vfo->setStepHz(slice->stepHz());

    // Phase 3P-I-a T15 — push board caps into VFO Flag so ANT buttons
    // hide on HL2/Atlas.
    // Phase 3P-I-b T9 — also push HPSDRModel so the BYPS button gates
    // correctly (ANAN10/ANAN8000D/G2/G2_1K suppress it despite hasRxBypassRelay).
    vfo->setBoardCapabilities(m_radioModel->boardCapabilities());
    vfo->setHpsdrSku(m_radioModel->hardwareProfile().model);
    vfo->setRxBypassActive(m_radioModel->alexController().rxOutOnTx());
    // Phase 3F closeout — give Slice A's VfoWidget the RadioModel pointer so
    // contextMenuEvent builds AntennaPickerMenu instead of the stub fallback.
    vfo->setRadioModel(m_radioModel);
    connect(m_radioModel, &RadioModel::currentRadioChanged, vfo,
            [this, vfo]() {
        vfo->setBoardCapabilities(m_radioModel->boardCapabilities());
        vfo->setHpsdrSku(m_radioModel->hardwareProfile().model);
    });

    // Phase 3F Sub-Epic C Task 9: VFO TX badge click → arbiter handoff.
    // VfoWidget emits txHandoffRequested(sliceIndex); MainWindow forwards to
    // RadioModel::txSliceArbiter()->requestHandoff(), which drops MOX before
    // flipping the TX-bound slice (RF-safe). Sub-Epic D wires the matching
    // reverse path (txBoundSliceChanged then updates all flag badges) in T10.

    // Phase 3F Sub-Epic E Task 4: VfoWidget context-menu intent signals.
    // Routes to RadioModel::requestSliceSampleRate / FilterPolicyDialog /
    // RadioModel::removeSlice. Phase 3F closeout: antennaChangeRequested is
    // now live, fired by AntennaPickerMenu (Sub-Epic E Task 5 consumer wire).
    //
    // Phase 3F Sub-Epic I closeout, defect G2: same rewire as the secondary
    // flags in createSliceFlag. The rate belongs to the DDC stream, so the
    // request goes to the stream rather than into a per-slice property
    // nothing downstream reads.
    // Phase 3F closeout — AntennaPickerMenu pick forwards to SliceModel::setRxAntenna.

    // Phase 3P-I-b T9 — VFO BYPS button ↔ AlexController::rxOutOnTx
    connect(&m_radioModel->alexController(), &AlexController::rxOutOnTxChanged,
            vfo, &VfoWidget::setRxBypassActive);

    // Stage C2: wire FilterPresetStore so VFO flag filter buttons use user overrides.
    vfo->setFilterPresetStore(m_radioModel->filterPresetStore());

    // 2026-05-11 bench: wire EOO-decoded RADE speaker callsign to the
    // VFO flag SNR row so "<call> ● <snr>dB" replaces "RADE ● <snr>dB"
    // whenever we have a decoded callsign for the current slice.  Sticky
    // until next decode replaces it; SliceModel clears the field when
    // setDspMode leaves RADE_U/RADE_L (A + D semantics per bench design
    // 2026-05-11).  Seed the current cached value once so a slice that
    // already holds a decoded callsign (e.g. from before the user opened
    // a panadapter container) paints correctly on first show.
    vfo->setRadeCallsign(slice->lastRadeRxCallsign());

    // --- Slice → spectrum display ---

    // VFO frequency change → move VFO marker
    // In CTUN mode (SmartSDR-style): pan stays fixed, VFO moves within it.
    // In traditional mode: pan follows VFO (auto-scroll handled in setVfoFrequency).
    // Band changes (large jumps) always recenter regardless of mode.


    // Task 42 (Phase 3P-II): reconfigure the Max Bin detector whenever the
    // IF passband changes so the passband-strongest-bin reading follows the
    // active filter window.
    //
    // 100 ms QTimer::singleShot debounce: rapid filter edge drags (e.g.
    // VFO flag drag) would otherwise call SetupDetectMaxBin on every
    // intermediate sample, which re-initialises the WDSP analyzer DSP
    // block at display-interrupt rate and wastes CPU.
    //
    // WdspEngine::setupMaxBinDetector wraps Thetis Console/dsp.cs:846-847
    // [@501e3f5] SetupDetectMaxBin; display channel = 0 (single panadapter).
    // Sample rate: primaryFftEngine()->sampleRate() at call time, which
    // reflects the currently active DDC bandwidth.  Primary engine because
    // the detector is set up with disp=0 (single display channel).
    // Frame rate: primaryFftEngine()->outputFps() * 1.1 matches Thetis
    //   console.cs:51150 [@501e3f5]: (int)Math.Max(1, _display_fps * 1.1f).
    connect(slice, &SliceModel::filterChanged, this, [this, slice](int low, int high) {
        QTimer::singleShot(100, this, [this, slice, low, high]() {
            FFTEngine* fft = primaryFftEngine();
            if (!m_radioModel || !fft) { return; }
            WdspEngine* eng = m_radioModel->wdspEngine();
            if (!eng) { return; }
            const double rate = fft->sampleRate();
            const int fps = qMax(1, static_cast<int>(fft->outputFps() * 1.1f));
            eng->setupMaxBinDetector(/*disp=*/0, /*ss=*/0, /*LO=*/0,
                                     rate,
                                     static_cast<double>(low),
                                     static_cast<double>(high),
                                     /*tauSeconds=*/0.5,
                                     fps);
            // Re-sync the CTUN slice offset after every setup call.  Filter
            // changes don't move the slice, but they re-run setupMaxBinDetector
            // and we want the offset to be authoritative against the current
            // slice freq vs DDC center -- not whatever stale offset was last
            // pushed by frequencyChanged.  Without this re-sync, a filter
            // change immediately after a CTUN tune could leave the detector
            // pointing at the wrong bins until the user nudges the VFO again.
            const double ddcCenter = activeSpectrumWidget()->ddcCenterFrequency();
            const double sliceFreq = slice ? slice->frequency() : ddcCenter;
            eng->setMaxBinSliceOffsetHz(/*disp=*/0, sliceFreq - ddcCenter);
        });
    });

    // Plan 4 D9 (Cluster E): initial TX mode push so the overlay has the right
    // IQ-space sign convention before the first paint.
    if (activeSpectrumWidget()) {
        activeSpectrumWidget()->setTxMode(slice->dspMode());
        // Initial XIT offset push + signal wires below so the TX overlay
        // centers on the actual TX frequency (RX VFO + XIT) rather than the
        // RX VFO alone.  Codex review feedback on PR #166.
        const int initialXitOffset = slice->xitEnabled() ? slice->xitHz() : 0;
        activeSpectrumWidget()->setTxVfoOffsetHz(initialXitOffset);
    }

    // XIT-enabled toggle and XIT-Hz changes both feed the spectrum's TX
    // overlay center.  When enabled flips off, the offset goes to zero;
    // when on, the offset tracks xitHz.
    auto pushXitOffset = [this, slice]() {
        if (!activeSpectrumWidget()) { return; }
        activeSpectrumWidget()->setTxVfoOffsetHz(slice->xitEnabled() ? slice->xitHz() : 0);
    };
    connect(slice, &SliceModel::xitEnabledChanged, this,
            [pushXitOffset](bool /*enabled*/) { pushXitOffset(); });
    connect(slice, &SliceModel::xitHzChanged, this,
            [pushXitOffset](int /*hz*/) { pushXitOffset(); });








    // --- SliceModel → VfoWidget: RIT/XIT inbound (S1.8a stubs) ---




    // --- SliceModel → VfoWidget: DSP tab inbound (S1.8b) ---

    // Sub-epic C-1: NR bank sync — VfoWidget::setSlice also connects activeNrChanged
    // via onActiveNrChanged for the full 7-button bank; this redundant connection is
    // removed to avoid double-firing. setSlice handles both initial sync and updates.
    // (Legacy setNr2Enabled call removed here — onActiveNrChanged covers NR2.)




    // --- VFO flag → slice ---




    // Shift+click on a filter preset on the flag — snap TX passband to
    // match the RX preset's audio Hz range (Thetis-style alignment shortcut).






    // NB cycling — nbModeCycled fires on user click; cycle the mode through
    // Off → NB → NB2 → Off via SliceModel. SliceModel's nbModeChanged feeds
    // back to setNbMode() (wired in the inbound block above).
    // From Thetis console.cs:43513 [v2.10.3.13].

    // NR/ANF → RxChannel directly (not SliceModel properties)

    // --- VfoWidget → SliceModel: DSP tab outbound (S1.8b) ---

    // --- SliceModel → VfoWidget: Audio tab inbound (S1.8c stubs) ---

    // --- VfoWidget → SliceModel: Audio tab outbound (S1.8c stubs) ---

    // --- VfoWidget → MainWindow: open Setup dialog to AGC/ALC page ---

    // --- VfoWidget → Setup → DSP → NB/SNB page (right-click on NB or SNB).
    // Mirrors Thetis chkNB_MouseDown / chkDSPNB2_MouseDown (console.cs:44447
    // [v2.10.3.13]) which call ShowSetupTab(NB_Tab).

    // --- VfoWidget → Setup → DSP → NR/ANF page (Task 18, Sub-epic C-1).
    // Emitted from DspParamPopup "More Settings…" on any NR bank button.
    // Mirrors Thetis chkNR_MouseDown (console.cs [v2.10.3.13]) which calls
    // ShowSetupTab(NR_Tab). Sub-tab selection per NrSlot is deferred to Task 17.

    // --- VfoWidget AUTO button → SliceModel auto-AGC toggle ---

    // --- SliceModel auto-AGC state → update visuals on both widgets ---

    // --- Noise floor fast-attack triggers (slice is guaranteed non-null here) ---
    {
        auto* nfTracker = m_radioModel->noiseFloorTracker();
        if (nfTracker) {
            // From Thetis v2.10.3.13 display.cs:905 — freq change > 0.5
            connect(slice, &SliceModel::frequencyChanged,
                    this, [nfTracker](double /*hz*/) {
                nfTracker->triggerFastAttack();
            });
            // From Thetis v2.10.3.13 display.cs:880 — mode change
            connect(slice, &SliceModel::dspModeChanged,
                    this, [nfTracker](NereusSDR::DSPMode /*mode*/) {
                nfTracker->triggerFastAttack();
            });
        }
    }

    // --- VfoWidget → SliceModel: RIT/XIT outbound (S1.8a stubs) ---




    // --- VfoWidget → SliceModel: STEP cycle (S1.8a — wires to live setStepHz) ---

    // --- VfoWidget → SliceModel: lock state (S1.8a — verifying edge exists) ---

    // --- SliceModel → VfoWidget: lock state inbound (S1.8a review — I3) ---
    // Without this edge, programmatic changes to SliceModel::locked (e.g. from
    // a future CAT/TCI command) would not be reflected in either lock button.

    // Phase 3F (Bug 2): wire Slice A's floating ✕ close button to removeSlice.
    // Slice A's close button is hidden by VfoWidget (last-slice invariant), so
    // this can only fire if a future change un-hides it; removeSlice refuses
    // to remove the final slice regardless, so this is safe and consistent
    // with the secondary-flag wiring in createSliceFlag().


    // The four spectrum controls that act on a slice (click-to-tune, filter-
    // edge drag, pan drag, CTUN toggle) used to be connected here, to
    // activeSpectrumWidget(), with lambdas capturing the `slice` above by
    // value. That pointer is Slice A and never moved, so on pan-0 all four
    // drove Slice A for the life of the session however many times the
    // operator selected another flag. Bench-caught 2026-07-28.
    //
    // They now live in wireSpectrumSliceControls, which ensureOverlayPanels
    // runs for every pan including this one, and which resolves its target
    // through sliceForPan(panId) on each signal instead of capturing it.
    // verify-no-captured-slice-spectrum-wiring.py keeps them from coming back.

    // --- dBm range strip → PanadapterModel (per-band grid storage + AppSettings) ---
    connect(activeSpectrumWidget(), &SpectrumWidget::dbmRangeChangeRequested,
            this, [this](float minDbm, float maxDbm) {
        if (m_radioModel && !m_radioModel->panadapters().isEmpty()) {
            PanadapterModel* pan = m_radioModel->panadapters().first();
            pan->setdBmFloor(static_cast<int>(minDbm));
            pan->setdBmCeiling(static_cast<int>(maxDbm));
        }
    });

    // Set initial lock state
    m_radioModel->receiverManager()->setDdcFrequencyLocked(
        activeSpectrumWidget()->ctunEnabled());

    // Position the VFO flag
    activeSpectrumWidget()->updateVfoPositions();

    // --- S-meter → VfoWidget level bar ---
    // MeterPoller emits smeterUpdated(double dbm) on each poll tick (100ms).
    // VfoWidget::setSmeter drives the VfoLevelBar S-meter indicator.
    if (m_meterPoller) {
        connect(m_meterPoller, &MeterPoller::smeterUpdated, vfo, &VfoWidget::setSmeter);
    }

    // --- Wire RxApplet to active slice ---
    if (m_rxApplet) {
        m_rxApplet->setSlice(slice);

        // AUTO button toggle → SliceModel
        connect(m_rxApplet, &RxApplet::autoAgcToggled,
                slice, &SliceModel::setAutoAgcEnabled);

        // Right-click AGC-T slider → open Setup dialog to AGC/ALC page
        connect(m_rxApplet, &RxApplet::openSetupRequested, this, [this]() {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(QStringLiteral("AGC/ALC"));
            dialog->show();
        });

        // RxApplet openNbSetupRequested wiring removed 2026-04-22 —
        // RxApplet no longer hosts any NB controls (strict Thetis parity).
        // VfoWidget::openNbSetupRequested above handles the NB→Setup hop.
    }

    // --- PhoneCwApplet → Setup → Transmit → DEXP/VOX page (Phase 3M-3a-iii Task 15).
    // Right-click on the DEXP [ON] button on the Phone tab opens the
    // SetupDialog and jumps to the DexpVoxPage leaf (Task 14).  Mirrors
    // the SpeechProcessorPage cross-link pattern (TransmitSetupPages.h:201).
    // (VOX-button right-click moved to TxApplet 2026-05-04 with the rest
    // of the VOX surface — see the TxApplet connect just below.)
    if (m_phoneCwApplet) {
        connect(m_phoneCwApplet, &PhoneCwApplet::openSetupRequested, this,
                [this](const QString& /*category*/, const QString& page) {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(page);
            dialog->show();
            dialog->raise();
        });
    }

    // --- TxApplet → Setup → Transmit → DEXP/VOX page (3M-3a-iii bench polish 2026-05-04).
    // Right-click on the VOX button (relocated from PhoneCwApplet) opens
    // the same DexpVoxPage leaf.  Same lambda body as the PhoneCwApplet
    // connect above.
    if (m_txApplet) {
        connect(m_txApplet, &TxApplet::openSetupRequested, this,
                [this](const QString& /*category*/, const QString& page) {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(page);
            dialog->show();
            dialog->raise();
        });
    }

    // --- Wire overlay Band flyout to RadioModel band-click handler (#118) ---
    // The signal still carries legacy (name, freqHz, mode) args for
    // backwards-compat with SpectrumOverlayPanel's kBands table, but the
    // handler now owns seed/restore policy — only the name is consulted.
    // Previously this lambda called setFrequency and silently discarded
    // the mode arg, which was the #118 reproducer (80m click moved VFO
    // but left mode stale).
    // Wired per strip in ensureOverlayPanels() now, so a band click acts on the
    // pan it was clicked on rather than on whichever slice happens to be
    // active. The handler there keeps this one's semantics (name only; the
    // legacy freqHz / mode args stay for SpectrumOverlayPanel's kBands table).

    // Phase 3P-II Task 65: notify PGXL of band changes so the amplifier can
    // switch its bias / antenna profile when the operator crosses a band boundary.
    // Gate: no-op if PGXL is not connected at the time of the band change.
    // Use the slice's actual frequency rather than a band center lookup because
    // Band.h has no centerFreqHz() helper (not needed elsewhere).
    connect(slice, &SliceModel::bandChanged,
            this, [this](NereusSDR::Band /*b*/) {
        PgxlConnection* pgxl = m_radioModel->pgxlConnection();
        if (!pgxl || !pgxl->isConnected()) { return; }
        SliceModel* s = m_radioModel->activeSlice();
        if (!s) { return; }
        pgxl->setBand(static_cast<int>(s->frequency()));
    });

    // Bench-fix 2026-05-19: also push on within-band frequency changes so
    // PGXL sees every tune, not just band-boundary crossings.
    // bandChanged fires only when the slice crosses a band edge; within-band
    // tunes (e.g. 7.200 -> 7.250 MHz) never trigger it, leaving PGXL's
    // bandA field stale until the operator crosses into an adjacent band.
    // Operator confirmed on-bench: after pairing PGXL via the serial field,
    // frequency changes from the tune wheel were not visible in PGXL status.
    // Debounced: a 200 ms QTimer::singleShot coalesces a burst of tune-wheel
    // clicks into one outbound command. m_pgxlBandPushTokenMs is the last
    // token; only the most recently scheduled callback fires the push.
    connect(slice, &SliceModel::frequencyChanged,
            this, [this](qint64 hz) {
        Q_UNUSED(hz);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_pgxlBandPushTokenMs = nowMs;
        QTimer::singleShot(200, this, [this, nowMs]() {
            if (m_pgxlBandPushTokenMs != nowMs) { return; }
            PgxlConnection* pgxl = m_radioModel->pgxlConnection();
            if (!pgxl || !pgxl->isConnected()) { return; }
            SliceModel* s = m_radioModel->activeSlice();
            if (!s) { return; }
            pgxl->setBand(static_cast<int>(s->frequency()));
        });
    });

    // SmartSDR API responder: also push slice freq/mode and TX state into the
    // TCP 4992 listener so PGXL/TGXL (acting as SmartSDR clients) pull current
    // band data via the documented API path rather than relying solely on the
    // explicit `flexradio band=N` push above. This is what `bsrcA=FLEX` on the
    // PGXL status frame consumes: the FlexRadio's slice 0 RF_frequency, mode,
    // and transmit MOX state.
    if (auto* l = m_radioModel->smartSdrListener()) {
        // Push current state immediately so the listener doesn't sit on
        // the constructor default (14.250 MHz USB) until the operator first
        // turns the dial. Without this, PGXL would see a misleading band
        // until the first frequency change.
        l->setSliceFrequencyHz(/*sliceId=*/0, slice->frequency());
        l->setSliceMode(/*sliceId=*/0, SliceModel::modeName(slice->dspMode()));
    }
    connect(slice, &SliceModel::frequencyChanged,
            this, [this](qint64 hz) {
        if (auto* l = m_radioModel->smartSdrListener()) {
            l->setSliceFrequencyHz(/*sliceId=*/0, hz);
        }
    });
    connect(slice, &SliceModel::dspModeChanged,
            this, [this](NereusSDR::DSPMode mode) {
        if (auto* l = m_radioModel->smartSdrListener()) {
            l->setSliceMode(/*sliceId=*/0, SliceModel::modeName(mode));
        }
    });

    // Phase 3P-II review fix C1: keep TunerApplet m_currentBand in sync so
    // right-click Save/Recall/Clear actions always address the actual current
    // (antenna, band) slot rather than the Band::Band20m default.
    if (m_tunerApplet) {
        connect(slice, &SliceModel::bandChanged,
                m_tunerApplet, &TunerApplet::setBand);
        // Seed with the slice's current band so the first context-menu open
        // before any band crossing is already correct.
        m_tunerApplet->setBand(bandFromFrequency(slice->frequency()));
    }
}

void MainWindow::reapplyRightStripDropPriority(bool force)
{
    // No work if the chrome bar isn't built yet (called pre-construction
    // from a stray signal) or the OverflowChip is missing.
    if (!m_chromeBarWidget || !m_overflowChip) { return; }

    // Hysteresis: if the strip width hasn't moved past a 30 px window
    // since our last decision and no content-change site flagged
    // force=true, return early. Without this gate, resizeEvent fires on
    // every pixel of a manual drag, and at boundary widths the
    // setVisible() toggles below re-fire resize at a slightly different
    // width, looping. Content-change call sites (voltage handler, ADC
    // overload timer) pass force=true since the budget hasn't moved but
    // the required width has — the deadband would otherwise skip work
    // we genuinely need.
    constexpr int kDeadbandPx = 30;
    const int gateBudget = m_chromeBarWidget->width();
    if (!force && m_rightStripSettled
        && qAbs(gateBudget - m_rightStripLastBudget) < kDeadbandPx) {
        return;
    }

    // Drop priority — spec §286-290:
    //   PA OK → CAT/TCI → PSU/PA → CPU → time
    // Each entry pairs the primary widget with its trailing separator
    // so they hide + show together (no dangling "··" runs). Time has
    // no trailing separator. CAT and TCI are listed as a pair per spec
    // ("CAT/TCI") and we drop them together to avoid the "TCI but no
    // CAT" half-state.
    struct DropEntry {
        QWidget* primary{nullptr};
        QWidget* sep{nullptr};
        QString  name;
    };
    const QVector<QVector<DropEntry>> priorityGroups = {
        // 1. PA OK badge — drops first (least operationally critical
        //    among the right-strip items per spec; user can still see
        //    fault state via the dialog or dashboard).
        {{ m_paStatusBadge, m_paStatusBadgeSep, tr("PA OK") }},
        // 2. CAT + TCI — drop together as a pair.
        {
            { m_catIndicator, m_catSep, tr("CAT") },
            { m_tciIndicator, m_tciSep, tr("TCI") },
        },
        // 3. PA telemetry tile (volts + temp stack; container is hidden
        //    when both rows are hidden, so this is a no-op for boards
        //    with no PA-V and no PA-T data — Atlas / Hermes / Angelia /
        //    Orion).
        {{ m_paStackWidget, m_paVoltLabelSep, tr("PA telemetry") }},
        // 4. CPU.
        {{ m_cpuMetric, m_cpuMetricSep, tr("CPU") }},
        // 5. Time — drops last; if it goes the strip is essentially empty.
        {{ m_timeWidget, nullptr, tr("Clock") }},
    };

    // Phase 1: restore everything (the window may have grown since the
    // last pass). Iterate every entry and force visible. The
    // OverflowChip itself drops its contents to start fresh.
    for (const auto& group : priorityGroups) {
        for (const auto& e : group) {
            if (e.primary) { e.primary->setVisible(true); }
            if (e.sep)     { e.sep->setVisible(true); }
        }
    }
    m_overflowChip->setDroppedItems({});

    // Force layout to recompute sizeHints with the new visibility state.
    if (auto* lay = m_chromeBarWidget->layout()) { lay->activate(); }

    auto* hbox = qobject_cast<QHBoxLayout*>(m_chromeBarWidget->layout());
    if (!hbox) { return; }

    // Helper: total width of all currently-visible non-stretch widgets +
    // their separators + the layout's spacings + content margins.
    auto requiredWidth = [hbox]() -> int {
        int w = 0;
        const int spacing = hbox->spacing();
        int visibleCount = 0;
        for (int i = 0; i < hbox->count(); ++i) {
            QLayoutItem* it = hbox->itemAt(i);
            if (auto* widget = it->widget()) {
                if (widget->isVisibleTo(widget->parentWidget())) {
                    w += widget->sizeHint().width();
                    ++visibleCount;
                }
            } else if (auto* sp = it->spacerItem()) {
                // Stretches don't add to required width; fixed spacers do.
                if (sp->expandingDirections() == Qt::Orientations()) {
                    w += sp->sizeHint().width();
                }
            }
        }
        if (visibleCount > 1) {
            w += spacing * (visibleCount - 1);
        }
        const auto m = hbox->contentsMargins();
        w += m.left() + m.right();
        return w;
    };

    const int budget = gateBudget;
    QStringList dropped;

    // Phase 2: drop priority groups in order until the strip fits.
    for (const auto& group : priorityGroups) {
        if (requiredWidth() <= budget) { break; }
        for (const auto& e : group) {
            if (e.primary) { e.primary->setVisible(false); }
            if (e.sep)     { e.sep->setVisible(false); }
            if (!e.name.isEmpty()) { dropped << e.name; }
        }
        if (auto* lay = m_chromeBarWidget->layout()) { lay->activate(); }
    }

    m_overflowChip->setDroppedItems(dropped);
    m_rightStripLastBudget = budget;
    m_rightStripSettled = true;
}

// ── CPU usage source toggle ──────────────────────────────────────────────────
// Right-click menu on the CPU MetricLabel — System / App radio choice.
// Mirrors Thetis's toolStripDropDownButton_CPU with systemToolStripMenuItem
// and thetisOnlyToolStripMenuItem (console.cs:44230-44247). Persists the
// choice in AppSettings under "CpuShowSystem".
void MainWindow::onCpuMenuRequested(const QPoint& localPos)
{
    if (!m_cpuMetric) { return; }

    QMenu menu(this);
    QAction* sysAct = menu.addAction(tr("System"));
    sysAct->setCheckable(true);
    sysAct->setChecked(m_cpuShowSystem);
    QAction* appAct = menu.addAction(tr("App (NereusSDR)"));
    appAct->setCheckable(true);
    appAct->setChecked(!m_cpuShowSystem);

    QAction* chosen = menu.exec(m_cpuMetric->mapToGlobal(localPos));
    if (!chosen) { return; }

    const bool newSys = (chosen == sysAct);
    if (newSys == m_cpuShowSystem) { return; }

    m_cpuShowSystem = newSys;
    AppSettings::instance().setValue(
        QStringLiteral("CpuShowSystem"),
        newSys ? QStringLiteral("True") : QStringLiteral("False"));

    // Reset delta state and smoothing so the next reading starts cleanly.
    m_cpuSmoothedPct = 0.0;
    m_cpuProcPrevWallUs = 0;
    m_cpuProcPrevUserUs = 0;
    m_cpuProcPrevSysUs = 0;
    m_cpuSysPrevTotal = 0;
    m_cpuSysPrevIdle = 0;
    m_cpuMetric->setValue(QStringLiteral("—"));
}

double MainWindow::readProcessCpuPercent()
{
    // Per-platform "process CPU time since boot" readers — return user +
    // kernel time consumed by this process expressed in microseconds.
    // POSIX (macOS / Linux) uses getrusage; Windows uses GetProcessTimes
    // and converts the FILETIME tick counter (100 ns) to microseconds.
    qint64       userUs = 0;
    qint64       sysUs  = 0;
    const qint64 nowUs  = QDateTime::currentMSecsSinceEpoch() * 1000LL;

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) != 0) { return 0.0; }
    auto toUs = [](const struct timeval& tv) -> qint64 {
        return static_cast<qint64>(tv.tv_sec) * 1'000'000LL
             + static_cast<qint64>(tv.tv_usec);
    };
    userUs = toUs(ru.ru_utime);
    sysUs  = toUs(ru.ru_stime);
#elif defined(Q_OS_WIN)
    FILETIME ftCreation{}, ftExit{}, ftKernel{}, ftUser{};
    if (!GetProcessTimes(GetCurrentProcess(),
                         &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        return 0.0;
    }
    auto fileTimeToUs = [](const FILETIME& ft) -> qint64 {
        ULARGE_INTEGER u{};
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return static_cast<qint64>(u.QuadPart / 10);  // 100 ns -> µs
    };
    userUs = fileTimeToUs(ftUser);
    sysUs  = fileTimeToUs(ftKernel);
#else
    return 0.0;
#endif

    if (m_cpuProcPrevWallUs == 0) {
        // First-call sentinel — capture baseline, return 0 this round.
        m_cpuProcPrevWallUs = nowUs;
        m_cpuProcPrevUserUs = userUs;
        m_cpuProcPrevSysUs  = sysUs;
        return 0.0;
    }

    const qint64 wallDelta = nowUs - m_cpuProcPrevWallUs;
    if (wallDelta <= 0) { return 0.0; }

    const qint64 cpuDelta = (userUs - m_cpuProcPrevUserUs)
                          + (sysUs  - m_cpuProcPrevSysUs);

    m_cpuProcPrevWallUs = nowUs;
    m_cpuProcPrevUserUs = userUs;
    m_cpuProcPrevSysUs  = sysUs;

    return 100.0 * static_cast<double>(cpuDelta)
                 / static_cast<double>(wallDelta);
}

double MainWindow::readSystemCpuPercent()
{
    // Per-platform "system CPU time since boot" readers. The CPU usage
    // formula is the same across all three: percent = 100 * (1 - dIdle / dTotal).
    // What differs is how each OS exposes the underlying tick counters.
    //
    // - macOS: host_processor_info(PROCESSOR_CPU_LOAD_INFO) → per-CPU
    //   tick counters; sum across cores.
    // - Linux: /proc/stat first line "cpu  user nice system idle iowait
    //   irq softirq steal guest guest_nice" — total = sum, idle = the
    //   `idle` field (not iowait, matching `top`/`htop` convention).
    // - Windows: GetSystemTimes → idle/kernel/user as FILETIMEs (100 ns).
    //   Note kernel time on Windows *includes* idle, so total = kernel +
    //   user; the percent formula above still holds.
    quint64 totalNow = 0;
    quint64 idleNow  = 0;

#if defined(Q_OS_MAC)
    natural_t                 cpuCount = 0;
    processor_info_array_t    info     = nullptr;
    mach_msg_type_number_t    numInfo  = 0;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                            &cpuCount, &info, &numInfo) != KERN_SUCCESS) {
        return 0.0;
    }

    auto* cpus = reinterpret_cast<processor_cpu_load_info_t>(info);
    for (natural_t i = 0; i < cpuCount; ++i) {
        for (int s = 0; s < CPU_STATE_MAX; ++s) {
            totalNow += cpus[i].cpu_ticks[s];
        }
        idleNow += cpus[i].cpu_ticks[CPU_STATE_IDLE];
    }

    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(info),
                  static_cast<vm_size_t>(numInfo) * sizeof(integer_t));
#elif defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/stat"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return 0.0; }
    const QByteArray line = f.readLine();
    f.close();

    // Tokenize on whitespace; first token is "cpu", remaining are tick
    // counts. Empty entries from the doubled space after "cpu" get filtered.
    const QList<QByteArray> rawParts = line.split(' ');
    QList<quint64> vals;
    vals.reserve(10);
    for (int i = 1; i < rawParts.size() && vals.size() < 10; ++i) {
        if (rawParts[i].isEmpty()) { continue; }
        bool ok = false;
        const quint64 v = rawParts[i].toULongLong(&ok);
        if (ok) { vals.append(v); }
    }
    if (vals.size() < 4) { return 0.0; }
    for (auto v : vals) { totalNow += v; }
    idleNow = vals[3];   // idle field; iowait NOT counted as idle (top convention)
#elif defined(Q_OS_WIN)
    FILETIME ftIdle{}, ftKernel{}, ftUser{};
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) { return 0.0; }
    auto fileTimeToTicks = [](const FILETIME& ft) -> quint64 {
        ULARGE_INTEGER u{};
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return static_cast<quint64>(u.QuadPart);
    };
    const quint64 idle   = fileTimeToTicks(ftIdle);
    const quint64 kernel = fileTimeToTicks(ftKernel);   // includes idle
    const quint64 user   = fileTimeToTicks(ftUser);
    totalNow = kernel + user;
    idleNow  = idle;
#else
    return 0.0;
#endif

    if (m_cpuSysPrevTotal == 0) {
        // First-call sentinel — capture baseline, return 0 this round.
        m_cpuSysPrevTotal = totalNow;
        m_cpuSysPrevIdle  = idleNow;
        return 0.0;
    }

    const quint64 totalDelta = totalNow - m_cpuSysPrevTotal;
    const quint64 idleDelta  = idleNow  - m_cpuSysPrevIdle;
    m_cpuSysPrevTotal = totalNow;
    m_cpuSysPrevIdle  = idleNow;

    if (totalDelta == 0) { return 0.0; }
    return 100.0 * (1.0 - static_cast<double>(idleDelta)
                              / static_cast<double>(totalDelta));
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // Update axis-lock positions for overlay-docked containers
    if (m_mainSplitter && m_containerManager) {
        // Use the spectrum pane (first splitter child) as reference
        QWidget* spectrumPane = m_mainSplitter->widget(0);
        if (spectrumPane) {
            m_hDelta = spectrumPane->width();
            m_vDelta = spectrumPane->height();
            m_containerManager->updateDockedPositions(m_hDelta, m_vDelta);
        }
    }

    // Re-run progressive drop on the right-side strip. Window grew →
    // restore items; window shrank → drop more. Spec §286-294.
    reapplyRightStripDropPriority();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Phase 3Q Sub-PR-4 D.3: TitleBar ConnectionSegment hover tooltip.
    // The segment has installEventFilter(this) in the D.2 wiring block.
    // We intercept QHelpEvent (ToolTip) and delegate to RadioModel for the
    // formatted multi-line string so the segment stays a thin paint layer.
    if (m_titleBar && watched == m_titleBar->connectionSegment()
     && event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        QToolTip::showText(helpEvent->globalPos(),
                           m_radioModel->buildConnectionTooltip(),
                           m_titleBar->connectionSegment());
        return true;
    }

    // PA-T row click → toggle persisted °C / °F preference. The widget
    // we watch is a MetricLabel (not a QLabel), so use the property
    // marker rather than a qobject_cast to a single concrete class.
    if (event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && w->property("isPaTempToggle").toBool()) {
            const PaTempUnit cur = PaTempUnitNotifier::currentUnit();
            const PaTempUnit next = (cur == PaTempUnit::Celsius)
                                      ? PaTempUnit::Fahrenheit
                                      : PaTempUnit::Celsius;
            PaTempUnitNotifier::setUnit(next);
            return true;  // event consumed
        }
    }

    // Phase 23: m_tciIndicator click → open Setup → TCI Server.
    // The indicator is a QWidget (not a QLabel) so we match by pointer identity.
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == m_tciIndicator) {
            openTciSetupPage();
            return true;  // event consumed
        }
    }

    // Handle ☰ panel toggle click — label has property "isPanelToggle"
    if (event->type() == QEvent::MouseButtonPress) {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label && label->property("isPanelToggle").toBool()) {
            // Toggle QSplitter right pane (index 1) visibility.
            // Save/restore sizes so spectrum expands when panel is hidden.
            if (!m_mainSplitter || m_mainSplitter->count() < 2) {
                return QMainWindow::eventFilter(watched, event);
            }
            QWidget* rightPane = m_mainSplitter->widget(1);
            if (rightPane->isVisible()) {
                // Hide: save current sizes, then collapse right to 0
                m_splitterSizesBeforeHide = m_mainSplitter->sizes();
                rightPane->hide();
                label->setStyleSheet(QStringLiteral(
                    "QLabel { color: #404858; font-weight: bold; font-size: 16px; }"));
            } else {
                // Show: restore saved sizes (or default 80/20 if none saved)
                rightPane->show();
                if (!m_splitterSizesBeforeHide.isEmpty()) {
                    m_mainSplitter->setSizes(m_splitterSizesBeforeHide);
                } else {
                    m_mainSplitter->setSizes({1024, 256});
                }
                label->setStyleSheet(QStringLiteral(
                    "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }"));
            }
            return true;  // event consumed
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #0f0f1a; }"
        "QMenuBar {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border-bottom: 1px solid #203040;"
        "}"
        "QMenuBar::item:selected { background: #00b4d8; }"
        "QMenu {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border: 1px solid #203040;"
        "}"
        "QMenu::item:selected { background: #00b4d8; }"
        "QLabel { color: #c8d8e8; }"
        "QStatusBar {"
        "  background: #1a2a3a;"
        "  color: #8090a0;"
        "  border-top: 1px solid #203040;"
        "}"));
}

void MainWindow::showConnectionPanel()
{
    if (!m_connectionPanel) {
        m_connectionPanel = new ConnectionPanel(m_radioModel, this);
        m_connectionPanel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_connectionPanel, &QObject::destroyed, this, [this]() {
            m_connectionPanel = nullptr;
        });
    }
    m_connectionPanel->show();
    m_connectionPanel->raise();
    m_connectionPanel->activateWindow();
}

// Phase 3Q Sub-PR-4 D.2 — right-click context menu on the TitleBar
// ConnectionSegment. "Reconnect" is intentionally absent: RadioModel has no
// public reconnect() API (tryAutoReconnect() is private to MainWindow and
// starts a full probe + discovery cycle, which is not appropriate to invoke
// from a context menu that the user might trigger mid-session). The user can
// use "Connect to other radio…" to re-select the same radio.
void MainWindow::showSegmentContextMenu(const QPoint& globalPos)
{
    QMenu menu(this);

    menu.addAction(tr("Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });
    menu.addAction(tr("Connect to other radio…"), this, [this]() {
        showConnectionPanel();
    });
    menu.addSeparator();
    menu.addAction(tr("Network diagnostics…"), this, [this]() {
        auto* dlg = new NetworkDiagnosticsDialog(
            m_radioModel, m_radioModel->audioEngine(), this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    menu.addSeparator();
    menu.addAction(tr("Copy IP address"), this, [this]() {
        QGuiApplication::clipboard()->setText(m_radioModel->connectionIpText());
    });
    menu.addAction(tr("Copy MAC address"), this, [this]() {
        QGuiApplication::clipboard()->setText(m_radioModel->connectionMacText());
    });

    menu.exec(globalPos);
}

void MainWindow::showStationContextMenu(const QPoint& globalPos)
{
    // Only show when connected — StationBlock only emits contextMenuRequested
    // in connected appearance, but guard here defensively.
    if (m_radioModel->connectionState() != ConnectionState::Connected) {
        return;
    }

    QMenu menu(this);

    menu.addAction(tr("Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });

    // "Edit radio…" — open ConnectionPanel so the user can edit the currently
    // connected radio's settings (model override, etc.). The panel pre-selects
    // by highlighted MAC when available; if not connected, user clicks the row.
    menu.addAction(tr("Edit radio…"), this, [this]() {
        showConnectionPanel();
        if (m_connectionPanel) {
            const QString mac =
                m_radioModel->connection()
                    ? m_radioModel->connection()->radioInfo().macAddress
                    : QString();
            if (!mac.isEmpty()) {
                m_connectionPanel->highlightMac(mac);
            }
        }
    });

    menu.addAction(tr("Forget radio"), this, [this]() {
        const QString mac =
            m_radioModel->connection()
                ? m_radioModel->connection()->radioInfo().macAddress
                : QString();
        m_radioModel->disconnectFromRadio();
        if (!mac.isEmpty()) {
            AppSettings::instance().forgetRadio(mac);
        }
    });

    menu.exec(globalPos);
}

void MainWindow::showSupportDialog()
{
    if (!m_supportDialog) {
        m_supportDialog = new SupportDialog(m_radioModel, this);
        m_supportDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_supportDialog, &QObject::destroyed, this, [this]() {
            m_supportDialog = nullptr;
        });
    }
    m_supportDialog->show();
    m_supportDialog->raise();
    m_supportDialog->activateWindow();
}

// Phase 3M-4 Task 8: open the modeless PureSignal dialog (Tools >
// PureSignal... and DSP > PureSignal...).  Lazy-constructs on the first
// call; subsequent calls show + raise the existing instance so geometry
// persists across opens.  Source-first port of Thetis console.cs:43099-
// 43104 linearityToolStripMenuItem_Click [v2.10.3.13]:
//
//   if (psform == null) psform = new PSForm(this);
//   psform.Show();
//   psform.Focus();
//
// NereusSDR mirrors via raise()+activateWindow() instead of Focus().
void MainWindow::openPureSignalDialog()
{
    if (!m_psForm) {
        // PureSignal coordinator is owned by RadioModel; pass it directly so
        // the dialog can wire signal/slot bindings even before connect.
        // RadioModel is the owner; we keep a non-owning pointer so the
        // dialog tolerates RadioModel-less startup (covered by tst_psform
        // construction-time test).
        PureSignal* coordinator =
            (m_radioModel ? m_radioModel->pureSignal() : nullptr);
        m_psForm = new PsForm(m_radioModel, coordinator, this);
    }
    m_psForm->show();
    m_psForm->raise();
    m_psForm->activateWindow();
}

// Phase 3J-2 H1: open the modeless SpotHubDialog.
//
// Lazy-constructs on first invocation, wiring all 7 spot-ingest clients
// + SpotModel + DxccColorProvider from RadioModel (Task H2 made these
// accessible via getters). Subsequent calls show + raise the existing
// instance so geometry, table sort state, and per-tab settings persist
// across opens. QPointer guards the pointer in case the dialog is ever
// deleted by some external path; lazy reconstruction is then automatic.
//
// Mirrors the modeless-singleton pattern at AetherSDR
// src/gui/MainWindow.cpp openDxClusterDialog() [@0cd4559].
void MainWindow::openSpotHub()
{
    if (!m_radioModel) { return; }
    if (!m_spotHubDialog) {
        m_spotHubDialog = new SpotHubDialog(
            m_radioModel->dxCluster(),
            m_radioModel->rbn(),
            m_radioModel->wsjtx(),
            m_radioModel->spotCollector(),
            m_radioModel->pota(),
            m_radioModel->freeDvReporter(),
            m_radioModel->pskReporter(),
            m_radioModel->spotModel(),
            m_radioModel->spotTableModel(),
            m_radioModel->dxccColorProvider(),
            this);
        // Bridge spotsClearedAll (Display tab's "Clear All Spots" button)
        // to SpotModel::clear so the global QShortcut and the dialog
        // button share one truth-source.
        connect(m_spotHubDialog.data(), &SpotHubDialog::spotsClearedAll,
                m_radioModel->spotModel(), &SpotModel::clear);
        // Spot List double-click tuneRequested(double Mhz) drives the
        // active slice. SliceModel::setFrequency takes Hz (double), so
        // multiply by 1e6 to convert MHz to Hz.
        connect(m_spotHubDialog.data(), &SpotHubDialog::tuneRequested,
                this, [this](double freqMhz) {
                    if (auto* slice = m_radioModel->activeSlice()) {
                        slice->setFrequency(freqMhz * 1.0e6);
                    }
                });
        // Phase 3J-2 + 3R M2: Display tab knob round-trip.
        // SpotHubDialog F4 writes every knob change to AppSettings and
        // emits settingsChanged. SpectrumWidget::loadSpotDisplaySettings
        // pulls the new values back out and pushes them into the spot
        // overlay setters in one go. Mirrors AetherSDR's refreshSpots
        // lambda (src/models/RadioModel.cpp [@0cd4559]) but the
        // NereusSDR shape lives on the widget so the test seam is local
        // (see tst_spothub_display_knobs).
        connect(m_spotHubDialog.data(), &SpotHubDialog::settingsChanged,
                this, [this] {
                    if (activeSpectrumWidget()) {
                        activeSpectrumWidget()->loadSpotDisplaySettings();
                    }
                });
        // Defensive re-seed.  The primary seed runs at MainWindow startup
        // (see the QTimer::singleShot(0, ...) lambda earlier in this file
        // that pairs loadSpotDisplaySettings + restoreSpotClientAutoStartState),
        // which guarantees the panadapter mask is populated before any
        // auto-started spot client emits its first spot.  This call is kept
        // as a belt-and-suspenders idempotent re-seed for the (rare) case
        // where the spectrum widget was not yet available at startup but
        // is now -- loadSpotDisplaySettings re-reads AppSettings and the
        // setter is no-op when the value is unchanged, so the cost is
        // bounded and the behaviour is correct either way.
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->loadSpotDisplaySettings();
        }

        // Phase 3R K-bench (bench feedback): wire the FreeDV tab's
        // Start / Stop button signals to the actual client lifecycle.
        // Previously the button emitted freedvStartRequested /
        // freedvStopRequested but nothing handled the Stop side, so
        // clicking Stop appeared to do nothing.
        if (auto* fdv = m_radioModel->freeDvReporter()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::freedvStartRequested,
                    fdv, &FreeDVReporterClient::startConnection);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::freedvStopRequested,
                    fdv, &FreeDVReporterClient::stopConnection);
        }

        // 2026-05-12 bench fix: wire the remaining 10 SpotHubDialog
        // lifecycle signals to their respective client methods.  Without
        // these connects the per-tab Connect / Start / Stop buttons in
        // SpotHubDialog emit signals into the void — the FreeDV pair
        // above was wired but DX Cluster, RBN, WSJT-X, SpotCollector,
        // and POTA buttons all silently no-op'd.  The auto-start path
        // (RadioModel::restoreSpotClientAutoStartState) calls the same
        // client methods directly and worked; only the manual-button
        // path was broken.  Clients themselves are correct — proven by
        // the spotReceived → spotModel wires at RadioModel.cpp:973-981.
        if (auto* dxc = m_radioModel->dxCluster()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::connectRequested,
                    dxc, &DxClusterClient::connectToCluster);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::disconnectRequested,
                    dxc, &DxClusterClient::disconnect);
        }
        if (auto* rbn = m_radioModel->rbn()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::rbnConnectRequested,
                    rbn, &DxClusterClient::connectToCluster);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::rbnDisconnectRequested,
                    rbn, &DxClusterClient::disconnect);
        }
        if (auto* wsjtx = m_radioModel->wsjtx()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::wsjtxStartRequested,
                    wsjtx, &WsjtxClient::startListening);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::wsjtxStopRequested,
                    wsjtx, &WsjtxClient::stopListening);
        }
        if (auto* sc = m_radioModel->spotCollector()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotCollectorStartRequested,
                    sc, &SpotCollectorClient::startListening);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotCollectorStopRequested,
                    sc, &SpotCollectorClient::stopListening);
        }
        if (auto* pota = m_radioModel->pota()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::potaStartRequested,
                    pota, &PotaClient::startPolling);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::potaStopRequested,
                    pota, &PotaClient::stopPolling);
        }

        // 2026-05-12 bench fix: PSK Reporter Start button source-first
        // port from freedv-gui.  The dialog emitted pskStartRequested
        // but nothing in MainWindow handled it.
        //
        // From freedv-gui main.cpp:2597 [@77e793a]:
        //   m_pskReporterTimer.Start(5 * 60 * 1000);
        // and main.cpp:1609-1616 [@77e793a]:
        //   if (timerId == ID_TIMER_PSKREPORTER) {
        //       for (auto& obj : wxGetApp().m_reporters) obj->send();
        //   }
        // PSK Reporter is a send-only IPFIX client (pskreporter.h:65-68
        // [@77e793a] — freqChange / transmit / inAnalogMode are no-ops).
        // "Start" = arm the 5-minute auto-send timer.
        //
        // From freedv-gui main.cpp:2694 [@77e793a]:
        //   m_pskReporterTimer.Stop();
        // "Stop" = disarm the timer.  Any queued records flush on
        // ~PskReporterClient when the client tears down (mirrors
        // pskreporter.cpp:171-181 [@77e793a]).
        if (auto* psk = m_radioModel->pskReporter()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::pskStartRequested,
                    psk, [psk](const QString& call,
                               const QString& grid) {
                        // 2026-05-12 bench fix (PR #238 review P2):
                        // apply the freshly-validated identity to the
                        // live client BEFORE arming the timer.  Without
                        // this call, the client keeps the (often empty)
                        // identity set at RadioModel construction time
                        // and emits IPFIX datagrams with empty receiver
                        // fields.  pskreporter.cpp:148-169 [@77e793a].
                        psk->setIdentity(
                            call, grid,
                            QStringLiteral("NereusSDR ") +
                                QStringLiteral(NEREUSSDR_VERSION));
                        psk->setAutoSendIntervalSec(
                            PskReporterClient::kReportingIntervalSec);
                    });
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::pskStopRequested,
                    psk, [psk]() {
                        psk->setAutoSendIntervalSec(0);
                    });
        }

        // 2026-05-12 bench fix: Save & Propagate writes User/GridSquare
        // to AppSettings but the FreeDVStationModel only reads its
        // m_ourGrid once at RadioModel construction.  Without this
        // forward the Reporter dialog's Distance + Hdg columns stay
        // zeroed until app restart.  Push the new grid into the model
        // on every save so distance/heading recompute live.
        connect(m_spotHubDialog.data(), &SpotHubDialog::identitySaved,
                this, [this](const QString& /*call*/,
                             const QString& grid,
                             const QString& /*msg*/) {
                    if (auto* sm = m_radioModel
                            ? m_radioModel->freeDvStationModel()
                            : nullptr) {
                        if (!grid.isEmpty()) {
                            sm->setOurGridSquare(grid);
                        }
                    }
                });

        // 2026-05-12 bench fix (Gap #6 — spot list ↔ panadapter hover sync).
        // Bidirectional: panadapter hover highlights the Spot List row,
        // Spot List hover paints a halo on the matching panadapter
        // label.  Lazy-wired here because both widgets are needed; the
        // dialog is constructed on first open.
        if (activeSpectrumWidget()) {
            connect(activeSpectrumWidget(), &SpectrumWidget::spotHoverIndexChanged,
                    m_spotHubDialog.data(),
                    &SpotHubDialog::setHoveredPanadapterSpot);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotListHoverChanged,
                    activeSpectrumWidget(),
                    &SpectrumWidget::setHoverSpotIndexExternal);
        }
    }
    m_spotHubDialog->show();
    m_spotHubDialog->raise();
    m_spotHubDialog->activateWindow();
}

// Phase 3J-2 H1: open the modeless FreeDVReporterDialog.
//
// Lazy-constructs on first invocation, wiring FreeDVStationModel +
// FreeDVReporterClient from RadioModel. Same singleton + show / raise
// pattern as openSpotHub.
//
// Wires three downstream connections:
//   qsyRequested -> FreeDVReporterClient::requestQSY (network QSY)
//   messageSendRequested -> FreeDVReporterClient::updateMessage
//   tuneRequested -> active SliceModel::setFrequency (local QSY)
//
// The dialog already calls setAttribute(Qt::WA_DeleteOnClose, false)
// in its own ctor so close + reopen preserves state.
void MainWindow::openFreeDVReporter()
{
    if (!m_radioModel) { return; }
    if (!m_freeDVReporterDialog) {
        m_freeDVReporterDialog = new FreeDVReporterDialog(
            m_radioModel->freeDvStationModel(),
            m_radioModel->freeDvReporter(),
            this);
        // QSY: dialog -> reporter client -> network broadcast.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::qsyRequested,
                m_radioModel->freeDvReporter(),
                &FreeDVReporterClient::requestQSY);
        // Message update: dialog -> reporter client.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::messageSendRequested,
                m_radioModel->freeDvReporter(),
                &FreeDVReporterClient::updateMessage);
        // Local QSY: dialog -> active slice tune. tuneRequested signature
        // is quint64 Hz so no MHz conversion needed.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::tuneRequested,
                this, [this](quint64 freqHz) {
                    if (auto* slice = m_radioModel->activeSlice()) {
                        slice->setFrequency(static_cast<double>(freqHz));
                    }
                });

        // Phase 3R K-bench (bench feedback): wire active slice VFO ->
        // dialog so the Band/Exact-freq filter actually tracks. Push
        // current value immediately + on every frequencyChanged.
        if (auto* slice = m_radioModel->activeSlice()) {
            m_freeDVReporterDialog->setActiveFrequency(
                static_cast<quint64>(slice->frequency()));
            connect(slice, &SliceModel::frequencyChanged,
                    m_freeDVReporterDialog.data(),
                    [this](double hz) {
                        if (m_freeDVReporterDialog) {
                            m_freeDVReporterDialog->setActiveFrequency(
                                static_cast<quint64>(hz));
                        }
                    });
        }
    }
    m_freeDVReporterDialog->show();
    m_freeDVReporterDialog->raise();
    m_freeDVReporterDialog->activateWindow();
}

// Phase 3F Sub-Epic D Task 10: build and exec the +PAN dropdown menu.
// Three sections, top to bottom:
//
//   1. "Add slice on active pan" -- one row per Slice letter (A..N) up
//      to maxSlices(); the next-available row triggers addSliceOnPan
//      on m_panStack's currently active pan id. Already-active letters
//      are greyed out so the operator can read at a glance which slice
//      is "next".
//   2. "Layout" -- one row per supported template (1 / 2v / 2h /
//      12h / 2x2). Current layout is checkmarked. Selecting one calls
//      PanadapterStack::applyLayout with a synthesized pan-id list
//      sized to that template's pan count.
//   3. "Float active pan..." -- detaches the active pan into its own
//      top-level window via PanadapterStack::floatPanadapter.
//
// All m_panStack-dependent actions guard on null so the menu opens
// cleanly even before Task 12 wires the stack. Add-slice's first
// triggerable row uses "pan-0" as a fallback active pan id pre-Task-12,
// matching the convention PanadapterStack uses for its bootstrap pan.
// Codex review round 3, PR #293. The template-to-pan-count table had three
// copies; this is the only one now.
QStringList MainWindow::panIdsForLayout(const QString& layoutId)
{
    // Pan-count per template: 1=1, 2v/2h=2, 12h=3, 2x2=4.
    const int needed = (layoutId == QStringLiteral("1"))   ? 1
                     : (layoutId == QStringLiteral("12h")) ? 3
                     : (layoutId == QStringLiteral("2x2")) ? 4
                                                           : 2;
    QStringList ids;
    for (int i = 0; i < needed; ++i) {
        ids << QStringLiteral("pan-%1").arg(i);
    }
    return ids;
}

// Codex review round 3, PR #293. See MainWindow.h for why this exists.
void MainWindow::applyPanLayout(const QString& layoutId)
{
    if (!m_panStack) { return; }

    const QStringList ids = panIdsForLayout(layoutId);

    qCInfo(lcContainer) << "Pan layout: applying" << layoutId << "with ids" << ids;
    m_panStack->applyLayout(layoutId, ids);

    if (!m_radioModel) { return; }

    // Shrink first. Slices left on panes applyLayout just deleted would keep a
    // dangling panKey, lose their VFO widget, and hold a DDC, a stream and
    // audio the operator can no longer reach. Running this before the grow
    // step also means the occupancy question below sees the settled answer.
    const int rehomed = m_radioModel->rehomeSlicesToPans(ids);
    if (rehomed > 0) {
        qCInfo(lcContainer) << "Layout: rehomed" << rehomed
                            << "slice(s) onto" << ids.value(0);
    }

    // Grow. Every pan wants a slice so that it has a VfoWidget and an RX
    // applet entry (Phase 3F bench fix 2026-06-03).
    //
    // Asked as an occupancy question rather than as
    // `for (i = slices().size(); i < target; ++i)`. That form assumed the
    // slice COUNT is the first unoccupied pan index, which stops being true
    // the moment slices co-host or get rehomed: shrinking a 2x2 to one pane
    // puts all four slices on pan-0, and expanding back then saw
    // existing == target, added nothing, and left three panes empty.
    // (Codex review round 4, PR #293.)
    //
    // No maxSlices arithmetic here: addSliceOnPan enforces the cap itself and
    // emits sliceAddRejected with an operator-facing reason when it cannot,
    // so restating it would be a second copy of that policy.
    //
    // Spread before creating. After a shrink every slice is co-hosted on
    // pan-0, so the slices these empty pans need already exist. Creating new
    // ones instead spends the maxSlices budget filling one pan and leaves the
    // rest empty with a surplus slice in the model. (Codex review round 5.)
    const int spread = m_radioModel->spreadSlicesOntoEmptyPans(ids);
    if (spread > 0) {
        qCInfo(lcContainer) << "Layout: spread" << spread
                            << "co-hosted slice(s) onto empty pans";
    }

    // Whatever is still empty after the surplus has been used up genuinely
    // needs a new slice.
    populateEmptyPans();
}

// ---------------------------------------------------------------------------
// populateEmptyPans — every pan needs a slice to be worth anything
//
// A pan with no slice has no VfoWidget, no RX applet entry, and no stream
// feeding it: it renders as an empty box with a 0.0000 flag.
//
// Called from two places, and the second one is why this is a function.
// applyPanLayout calls it because a layout change can add panes. The connect
// handler calls it because the startup layout restore deliberately does NOT
// (MainWindow.cpp, the PanLayoutId block): at startup no radio is connected
// and the stream pool is unsized, so manufacturing slices there would bind
// nothing. Correct as far as it goes, but nothing finished the job once a
// radio did connect.
//
// Bench-caught 2026-08-01 (J.J. Boyd, KG4VCF): quit with a 2v layout, relaunch,
// connect, and the second pan is permanently dead until the operator notices
// they have to add Slice B by hand. The log gives it away by omission, with no
// "Pan layout: applying" line anywhere in the session.
//
// addSliceOnPan enforces the maxSlices cap itself and emits sliceAddRejected
// with an operator-facing reason, so there is no cap arithmetic here.
// ---------------------------------------------------------------------------
void MainWindow::populateEmptyPans()
{
    if (!m_radioModel || !m_panStack) { return; }

    // The pans that actually exist, not panIdsForLayout's template. After a
    // restore those are the same, but reading the live stack means a pan
    // created by any other route is covered too.
    QStringList ids;
    for (const PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet) { ids << applet->panId(); }
    }

    for (const QString& emptyPan : m_radioModel->pansWithoutSlices(ids)) {
        m_radioModel->addSliceOnPan(emptyPan);
    }
}

void MainWindow::showPanMenu()
{
    QMenu menu(this);

    // -- Add slice section --------------------------------------------
    menu.addSection(QStringLiteral("Add slice on active pan"));
    if (m_radioModel) {
        const int currentSlices = m_radioModel->slices().size();
        const int maxS          = m_radioModel->maxSlices();
        for (int i = 0; i < maxS; ++i) {
            const QChar letter = QChar(QLatin1Char('A' + i));
            QAction* act = menu.addAction(QStringLiteral("Slice %1").arg(letter));
            // Already-active letters: grey. Next-available letter:
            // wired. Beyond next-available: also grey (one click ->
            // one slice).
            act->setEnabled(i == currentSlices);
            if (i == currentSlices) {
                connect(act, &QAction::triggered, this, [this]() {
                    if (!m_radioModel) { return; }
                    const QString activePan = m_panStack
                                                 ? m_panStack->activePanId()
                                                 : QStringLiteral("pan-0");
                    m_radioModel->addSliceOnPan(activePan);
                });
            }
        }
    }

    // -- Layout section -----------------------------------------------
    menu.addSeparator();
    menu.addSection(QStringLiteral("Layout"));
    const QStringList layouts = {
        QStringLiteral("1"),
        QStringLiteral("2v"),
        QStringLiteral("2h"),
        QStringLiteral("12h"),
        QStringLiteral("2x2"),
    };
    for (const QString& layoutId : layouts) {
        QAction* act = menu.addAction(layoutId);
        if (m_panStack && m_panStack->currentLayoutId() == layoutId) {
            act->setCheckable(true);
            act->setChecked(true);
        }
        connect(act, &QAction::triggered, this, [this, layoutId]() {
            applyPanLayout(layoutId);
        });
    }

    // -- Float active pan section -------------------------------------
    menu.addSeparator();
    QAction* floatAct = menu.addAction(QStringLiteral("Float active pan..."));
    floatAct->setEnabled(m_panStack != nullptr);
    connect(floatAct, &QAction::triggered, this, [this]() {
        if (!m_panStack) { return; }
        m_panStack->floatPanadapter(m_panStack->activePanId());
    });

    menu.exec(QCursor::pos());
}

// Phase 3M-4 bench-fix: PSA bottom-banner indicator visibility
// gated on (caps.hasPureSignal && PureSignal::isAutoCalEnabled).
// Centralised so onConnectionStateChanged + autoCalEnabledChanged +
// pureSignalCoordinatorReady can all share one truth-source.
void MainWindow::updatePsaIndicatorVisibility()
{
    if (!m_psaIndicator) { return; }
    const bool caps =
        m_radioModel
        && m_radioModel->isConnected()
        && m_radioModel->boardCapabilities().hasPureSignal;
    auto* ps = m_radioModel ? m_radioModel->pureSignal() : nullptr;
    const bool armed = ps && ps->isAutoCalEnabled();
    m_psaIndicator->setVisible(caps && armed);
}

void MainWindow::showAudioDiagnoseDialog()
{
#if defined(Q_OS_LINUX)
    AudioEngine* eng = m_radioModel->audioEngine();
    if (!eng) {
        return;
    }
    auto* dlg = new VaxLinuxFirstRunDialog(eng, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
#endif
}

void MainWindow::onConnectionStateChanged()
{
    // Phase 3Q-8: forward state to the spectrum widgets for the disconnect
    // overlay.
    //
    // EVERY pan, not just the active one. SpectrumWidget::mousePressEvent
    // opens with a disconnected guard that emits disconnectedClickRequest()
    // and returns, so a widget left at its Disconnected default swallows every
    // press: no click-to-tune, no filter-edge drag, no pan drag. Only
    // mouseMoveEvent is ungated, which is why a second pan still tracked the
    // cursor readout and looked alive while being completely unclickable --
    // and why tuning appeared to work only with the pointer over the flag,
    // which is a separate widget with its own handlers.
    pushConnectionStateToPans();

    if (m_radioModel->isConnected()) {
        // Board widget top line: show model code ("Saturn") not marketing name
        // ("ANAN-G2 (Saturn)") — the marketing name truncates at status-bar widths.
        // boardCodeName() returns the HPSDRHW enum label which is short and unambiguous.
        {
            const HPSDRHW board = m_radioModel->connection()->radioInfo().boardType;
            const QString code  = QString::fromLatin1(boardCodeName(board));
            m_radioModelLabel->setText(code);
        }
        m_radioModelLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #c8d8e8; font-size: 12px; }"));
        // Firmware: "v27" not "FW 27" — shorter, fits the compact status bar.
        m_radioFwLabel->setText(QStringLiteral("v%1").arg(m_radioModel->version()));
        m_radioFwLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 12px; }"));

        // Phase 3Q-6/D.1: setRadio() removed — radio identity moves to the
        // STATION block (sub-PR-7). Segment state is already driven by
        // connectionStateChanged → ConnectionSegment::setState (see D.2 wiring
        // block in the constructor).

        // Phase 3Q Sub-PR-6 (F.1): RxDashboard is always bound to slice(0)
        // from buildStatusBar(). No per-connect rebind needed — the slice
        // stays the same object across connect/disconnect cycles.
        // (Connection details moved to segment tooltip / NetworkDiagnosticsDialog.)

        // Wire step attenuator controller to the live radio connection
        // and set max attenuation from board capabilities.
        // From Thetis console.cs ucInfoBar Warning() + SetupForm attenuator init.
        m_stepAttController->setRadioConnection(m_radioModel->connection());
        const auto& caps = BoardCapsTable::forBoard(
            m_radioModel->connection()->radioInfo().boardType);
        m_stepAttController->setMaxAttenuation(caps.attenuator.maxDb);
        // Wire HPSDR-board flag — Atlas/Metis kit uses preamp save/restore on
        // MOX rather than per-band TX ATT (Thetis console.cs:29548 [v2.10.3.13]:
        //   if (HardwareSpecific.Model == HPSDRModel.HPSDR) { ... }).
        m_stepAttController->setIsHpsdrBoard(
            m_radioModel->connection()->radioInfo().boardType == HPSDRHW::Atlas);

        // Phase 3M-4 Task 10 + bench-fix: PSA bottom-banner indicator is
        // gated on caps.hasPureSignal AND pureSignal->isAutoCalEnabled().
        // Boards without PS support (HL2 / Atlas) hide the FB+PS pair
        // entirely; PS-capable boards (Hermes II / Angelia / Orion /
        // Saturn / G2) show it only when the user has armed PS-A.
        // updatePsaIndicatorVisibility centralises the condition.
        updatePsaIndicatorVisibility();
        // Phase 3M-4 Task 13: gate PureSignalApplet + TxApplet [PS-A] on
        // the same board capability.  PureSignalApplet hides itself; the
        // TxApplet [PS-A] button hides via its setBoardCapabilities slot.
        if (m_pureSignalApplet) {
            m_pureSignalApplet->setVisible(caps.hasPureSignal);
        }
        if (m_txApplet) {
            m_txApplet->setBoardCapabilities(caps);
        }
        // P1 full-parity §4.1: gate AutoAttMode::Adaptive on per-step
        // calibration support.  Must be set BEFORE loadSettings() so a
        // persisted "Adaptive" string is clamped to Classic when the
        // connected board lacks the feature.
        m_stepAttController->setHasStepAttenuatorCal(caps.hasStepAttenuatorCal);
        m_stepAttController->loadSettings(m_radioModel->connection()->radioInfo().macAddress);

        // Phase 3Q Task 5 — auto-close: 1 s after connect, accept() the panel if open.
        // Fires on transitions TO Connected only (not on repeated Connected emits).
        if (m_connectionPanel && m_connectionPanel->isVisible()) {
            QTimer::singleShot(1000, this, [this]() {
                if (m_connectionPanel && m_connectionPanel->isVisible()
                    && m_radioModel->isConnected()) {
                    m_connectionPanel->accept();
                }
            });
        }

        // Per-radio peripherals refactor (2026-05-26): the PGXL / TGXL
        // auto-connect-on-Connected block previously lived here in
        // MainWindow but read GLOBAL AppSettings keys.  The lifecycle
        // (gated on the per-MAC FourO3A flag) now lives in
        // RadioModel::applyPeripheralsForCurrentMac(), which is driven
        // from onConnectionStateChanged so MainWindow doesn't need to
        // touch the peripheral wires here.

        // Phase 3P-II Task 20: wire AmpApplet controls to PgxlConnection.
        // operateToggled: translate bool to "operate"/"standby" command string.
        // statusUpdated: fan the k=v map into AmpApplet setter slots.
        // These connects are made on every radio-connect. Qt lambda
        // connects do not support UniqueConnection, so guard with a flag
        // to avoid stacking connections across reconnects. The flag is
        // instance-local; resetFlag is intentional (first time = wire).
        if (m_ampApplet && !m_ampAppletWired) {
            m_ampAppletWired = true;

            connect(m_ampApplet, &AmpApplet::operateToggled,
                    this, [this](bool wantOperate) {
                // Bench-fix 2026-05-19: pcap stream 11 (.19 PowerGeniusDesktop
                // -> .235 PGXL :9008) shows the actually-used wire command
                // for OPERATE is `operate=1` (key=value), not bare `operate`.
                // PGXL rejected `operate` / `standby` with error 50000016
                // every click.
                m_radioModel->pgxlConnection()->sendCommand(
                    wantOperate ? QStringLiteral("operate=1")
                                : QStringLiteral("operate=0"));
            });

            connect(m_radioModel->pgxlConnection(),
                    &PgxlConnection::statusUpdated,
                    this, [this](const QMap<QString, QString>& kvs) {
                if (kvs.contains(QStringLiteral("temp")))
                    m_ampApplet->setTemp(kvs.value(QStringLiteral("temp")).toFloat());
                if (kvs.contains(QStringLiteral("id")))
                    m_ampApplet->setDrainCurrent(kvs.value(QStringLiteral("id")).toFloat());
                if (kvs.contains(QStringLiteral("vac")))
                    m_ampApplet->setMainsVoltage(kvs.value(QStringLiteral("vac")).toInt());
                if (kvs.contains(QStringLiteral("state")))
                    m_ampApplet->setState(kvs.value(QStringLiteral("state")));
                if (kvs.contains(QStringLiteral("meffa")))
                    m_ampApplet->setMeff(kvs.value(QStringLiteral("meffa")));

                // 2026-05-22 bench fix: PGXL's `peakfwd` and `swr`
                // status fields are HOLD values that latch the last TX
                // peak and DO NOT decay back to 0 when the amp leaves
                // TRANSMIT_A/B (PGXL's intent is "show the last QSO's
                // peak on the front panel"). For our applet gauges we
                // want the live keyed value during TX and a clean zero
                // between cycles, so we gate the peakfwd / swr writes
                // on the transmitting state. Without this gate the
                // previous bench-fix at this site (which forced fwd=0
                // / swr=1 when state changed to IDLE) was overwritten
                // 30 ms later by the next status response carrying the
                // stale latched peakfwd.
                //
                // Inferred transmitting state: if the status update
                // includes state=, use it; otherwise fall back to the
                // last cached state (m_ampApplet tracks it via
                // setState).
                bool transmitting = false;
                if (kvs.contains(QStringLiteral("state"))) {
                    const QString st = kvs.value(QStringLiteral("state"));
                    transmitting =
                        (st == QStringLiteral("TRANSMIT_A")
                         || st == QStringLiteral("TRANSMIT_B"));
                    if (!transmitting) {
                        m_ampApplet->setFwdPower(0.0f);
                        m_ampApplet->setSwr(1.0f);
                    }
                } else {
                    transmitting = m_ampApplet->isTransmitting();
                }

                // 2026-05-20 bench fix: peakfwd is dBm (not watts) and swr
                // is signed dB return loss (not an SWR ratio). Convert
                // here so the AmpApplet gauges read the same numbers
                // the SMeterWidget already gets via
                // RadioModel::ampMetersChanged.
                // 2026-05-22 bench fix: only forward the converted
                // peakfwd / swr when the amp is actually transmitting;
                // otherwise the stale latched peak would overwrite the
                // zero set by the state-edge block above.
                if (transmitting && kvs.contains(QStringLiteral("peakfwd"))) {
                    const float dbm   = kvs.value(QStringLiteral("peakfwd")).toFloat();
                    const float watts = std::pow(10.0f, dbm / 10.0f) / 1000.0f;
                    m_ampApplet->setFwdPower(watts);
                }
                if (transmitting && kvs.contains(QStringLiteral("swr"))) {
                    const float rlDbWire =
                        kvs.value(QStringLiteral("swr")).toFloat();
                    float ratio;
                    if (rlDbWire >= 0.0f) {
                        ratio = 99.0f;  // RL>=0 -> open/short, cap display
                    } else {
                        const float gamma = std::pow(10.0f, rlDbWire / 20.0f);
                        ratio = (gamma >= 0.999f)
                            ? 99.0f
                            : (1.0f + gamma) / (1.0f - gamma);
                    }
                    m_ampApplet->setSwr(ratio);
                }
            });

            // Phase 3P-II Phase 4 Task 88: track PGXL connected state for the
            // context menu Disconnect/Reconnect label.
            connect(m_radioModel->pgxlConnection(), &PgxlConnection::connected,
                    this, [this]() { m_ampApplet->setPgxlConnected(true); });
            connect(m_radioModel->pgxlConnection(), &PgxlConnection::disconnected,
                    this, [this]() { m_ampApplet->setPgxlConnected(false); });

            // Phase 3P-II Phase 4 Task 88: context menu right-click signals.

            // connectionToggleRequested: disconnect or reconnect PGXL.
            connect(m_ampApplet, &AmpApplet::connectionToggleRequested,
                    this, [this]() {
                PgxlConnection* pgxl = m_radioModel->pgxlConnection();
                if (!pgxl) { return; }
                if (pgxl->isConnected()) {
                    pgxl->disconnect();
                } else {
                    // PR #279 review #5 (2026-05-23): use the same
                    // PGXL_ManualIp / PGXL_ManualPort keys that
                    // auto-connect at line ~6331 + the Setup ->
                    // CAT & Network -> PGXL page persist.  The
                    // earlier reads of obsolete PGXL_IpAddress /
                    // PGXL_Port (default 50001) returned empty
                    // strings on every install that had only ever
                    // written the canonical keys, so the AmpApplet
                    // context-menu Connect did nothing.
                    //
                    // Per-radio peripherals refactor (2026-05-26):
                    // the keys are scoped under
                    // hardware/<mac>/peripherals/.  Default 9008
                    // matches the AppSettings.h documented default.
                    const QString ip = m_radioModel->peripheralValue(
                        QStringLiteral("PGXL_ManualIp"));
                    const quint16 port = static_cast<quint16>(
                        m_radioModel->peripheralValue(
                            QStringLiteral("PGXL_ManualPort"),
                            QStringLiteral("9008")).toUInt());
                    if (!ip.isEmpty()) {
                        pgxl->connectToPgxl(ip, port);
                    }
                }
            });

            // diagnosticsCopyRequested: build a brief diagnostic string and copy to clipboard.
            connect(m_ampApplet, &AmpApplet::diagnosticsCopyRequested,
                    this, [this]() {
                PgxlConnection* pgxl = m_radioModel->pgxlConnection();
                const QString text = QStringLiteral(
                    "PGXL Diagnostics\n"
                    "Connected: %1\n"
                    "IP: %2\n"
                ).arg(pgxl && pgxl->isConnected() ? QStringLiteral("Yes") : QStringLiteral("No"))
                 .arg(pgxl ? pgxl->peerAddress() : QStringLiteral("--"));
                QGuiApplication::clipboard()->setText(text);
            });

            // Phase 3P-II Phase 4 Task 90: wire navigationRequested to openSetup().
            connect(m_ampApplet, &AmpApplet::navigationRequested,
                    this, &MainWindow::openSetup);
        }

        // Phase 3P-II Phase 4 Task 89: wire TunerApplet context menu signals to
        // TgxlConnection. buildUI() runs once at startup, so no deduplication
        // guard is needed. Qt::UniqueConnection is intentionally NOT used here:
        // Qt6 silently no-ops UniqueConnection when the slot is a lambda (it
        // requires a pointer-to-member-function of a QObject subclass), so
        // all four connects below would have been dead on arrival.
        if (m_tunerApplet) {
            // Track TGXL connected state for Disconnect/Reconnect label.
            connect(m_radioModel->tgxlConnection(), &TgxlConnection::connected,
                    this, [this]() { m_tunerApplet->setTgxlConnected(true); });
            connect(m_radioModel->tgxlConnection(), &TgxlConnection::disconnected,
                    this, [this]() { m_tunerApplet->setTgxlConnected(false); });

            // connectionToggleRequested: disconnect or reconnect TGXL.
            connect(m_tunerApplet, &TunerApplet::connectionToggleRequested,
                    this, [this]() {
                TgxlConnection* tgxl = m_radioModel->tgxlConnection();
                if (!tgxl) { return; }
                if (tgxl->isConnected()) {
                    tgxl->disconnect();
                } else {
                    // Per-radio peripherals refactor (2026-05-26): keys
                    // scoped under hardware/<mac>/peripherals/.
                    const QString ip = m_radioModel->peripheralValue(
                        QStringLiteral("TGXL_ManualIp"));
                    const quint16 port = static_cast<quint16>(
                        m_radioModel->peripheralValue(
                            QStringLiteral("TGXL_ManualPort"),
                            QStringLiteral("9010")).toUInt());
                    if (!ip.isEmpty()) {
                        tgxl->connectToTgxl(ip, port);
                    }
                }
            });

            // diagnosticsCopyRequested: build diagnostic string and copy to clipboard.
            connect(m_tunerApplet, &TunerApplet::diagnosticsCopyRequested,
                    this, [this]() {
                TgxlConnection* tgxl = m_radioModel->tgxlConnection();
                const QString text = QStringLiteral(
                    "TGXL Diagnostics\n"
                    "Connected: %1\n"
                    "IP: %2\n"
                ).arg(tgxl && tgxl->isConnected() ? QStringLiteral("Yes") : QStringLiteral("No"))
                 .arg(tgxl ? tgxl->peerAddress() : QStringLiteral("--"));
                QGuiApplication::clipboard()->setText(text);
            });

            // Phase 3P-II Phase 4 Task 90: wire navigationRequested to openSetup().
            connect(m_tunerApplet, &TunerApplet::navigationRequested,
                    this, &MainWindow::openSetup,
                    Qt::UniqueConnection);
        }
    } else {
        m_radioModelLabel->setText(QStringLiteral("—"));
        m_radioModelLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 12px; }"));
        m_radioFwLabel->setText(QStringLiteral("—"));
        m_radioFwLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 12px; }"));

        // Save step attenuator settings before disconnecting.
        if (m_radioModel->connection()) {
            m_stepAttController->saveSettings(m_radioModel->connection()->radioInfo().macAddress);
        }

        // Disconnect step attenuator from radio
        m_stepAttController->setRadioConnection(nullptr);

        // Phase 3M-4 Task 10 + bench-fix: hide the PSA indicator on
        // disconnect.  Re-evaluated via updatePsaIndicatorVisibility on
        // next reconnect (which now also checks PureSignal::isAutoCalEnabled).
        updatePsaIndicatorVisibility();
        // Phase 3M-4 Task 13: hide PureSignalApplet + TxApplet [PS-A] on
        // disconnect.  Same lifetime model as the PSA indicator above.
        // Re-evaluation happens on next reconnect via the connected-branch
        // gating block.
        if (m_pureSignalApplet) {
            m_pureSignalApplet->setVisible(false);
        }
        if (m_txApplet) {
            // Push the unknown-board defaults (hasPureSignal == false) so
            // [PS-A] hides.  RadioModel::boardCapabilities() returns the
            // unknown-board fallback when m_hardwareProfile.caps is null
            // (RadioModel.cpp:1016 [v2.10.3.13] equivalent).
            m_txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
        }

        // Phase 3Q Sub-PR-6 (F.1): RxDashboard shows placeholder "—" when
        // disconnected automatically (slice values reset to defaults). No
        // per-disconnect update needed here.
        // (The "last connected" breadcrumb moved to the segment tooltip in D.2.)

        // Phase 3Q Task 5 — auto-open: on disconnect (after having been connected),
        // open the ConnectionPanel so the user can reconnect.
        // Guard: m_autoReconnectInProgress suppresses the panel during background
        // auto-reconnect (Task 17); m_shuttingDown suppresses it during ⌘Q so
        // ConnectionPanel's ctor doesn't restart discovery mid-close (would
        // beach-ball the close path for the full SafeDefault scan window — see
        // [shutdown-trace] log analysis 2026-05-02). The very first state read
        // at startup is Disconnected which should not open the panel either —
        // the radio-name check below handles that case.
        // The panel itself is non-modal (show/raise), matching the current pattern.
        if (!m_autoReconnectInProgress && !m_shuttingDown) {
            // Only open if we were previously connected (transition from Connected,
            // not the initial Disconnected state at startup). We detect this by
            // checking if the model has ever reported a radio name — set on connect.
            if (!m_radioModel->name().isEmpty()) {
                showConnectionPanel();
            }
        }
    }

    // 3Q-9 (post-feedback simplification): Connect is "reconnect to last".
    // Greyed out when (a) we're already connected, OR (b) there's no
    // last-used radio in saved entries to reconnect to. Manage Radios is
    // the only way to pick a different radio.
    //
    // Use the model's authoritative connectionState (3Q-1) rather than
    // RadioModel::isConnected() — the latter dereferences m_connection
    // which can briefly disagree during teardown (m_connectionState
    // already Disconnected but m_connection->isConnected() still true
    // until the worker-thread teardown finishes). Without this, a
    // Radio→Disconnect would leave Connect greyed forever.
    if (m_actConnect && m_actDisconnect && m_actProtocolInfo) {
        const bool connected =
            (m_radioModel->connectionState() == ConnectionState::Connected);
        AppSettings& s = AppSettings::instance();
        const QString lastMac = s.lastConnected();
        const bool hasReconnectTarget =
            !connected
            && !lastMac.isEmpty()
            && s.savedRadio(lastMac).has_value();
        m_actConnect->setEnabled(hasReconnectTarget);
        m_actDisconnect->setEnabled(connected);
        m_actProtocolInfo->setEnabled(connected);
    }
}

// Phase 3I Task 17 / Phase 3Q Task 10 — auto-reconnect on launch.
//
// Logic:
//   1. Collect ALL saved radios with autoConnect = true. If none, open the
//      ConnectionPanel (Phase 3Q polish — design §6.1 cold-launch flow) so
//      the user has a one-click path to a saved radio or to Add Manually.
//   2. Pick the target MAC:
//      - Single autoConnect entry → use it directly.
//      - Multiple entries → most-recently-connected MAC wins (radios/lastConnected);
//        a one-time status-bar warning is posted via RadioModel::autoConnectAmbiguous.
//   3a. pinToMac=true  → run a Fast-profile discovery, connect when the same MAC
//      is seen. A 3-second kill timer fires if the MAC never appears; on timeout
//      the ConnectionPanel opens with the target row highlighted and a status-bar
//      message explains why.  RadioModel's m_autoConnectInProgress flag ensures
//      the RadioConnection::connectFailed signal (if the radio replies but fails)
//      also surfaces via RadioModel::autoConnectFailed → MainWindow lambdas.
//   3b. pinToMac=false → direct connect to saved IP. Arm m_autoConnectInProgress
//      so RadioConnection::connectFailed is forwarded as autoConnectFailed.
//
// m_autoReconnectInProgress gates the disconnect auto-open in onConnectionStateChanged
// so the background probe does not flash the ConnectionPanel while in flight.
void MainWindow::tryAutoReconnect()
{
    AppSettings& s = AppSettings::instance();

    // --- Step 1: Collect all autoConnect-flagged saved radios ---
    const QList<SavedRadio> allSaved = s.savedRadios();
    QStringList autoMacs;
    for (const SavedRadio& sr : allSaved) {
        if (sr.autoConnect) {
            autoMacs << sr.info.macAddress;
        }
    }
    if (autoMacs.isEmpty()) {
        // Phase 3Q polish: cold-launch panel auto-open. Design §6.1 — when
        // there's no auto-connect target, surface the radio list so the
        // user has a one-click path to either connect (saved radio shown)
        // or add one (empty list). Non-modal so the app remains usable
        // around the panel. Skipped when an auto-connect attempt is in
        // flight (the auto-reconnect path handles its own panel open via
        // the failure handler in Task 10).
        showConnectionPanel();
        return;
    }

    // --- Step 2: Pick the target MAC (most-recently-connected wins) ---
    const QString lastMac = s.lastConnected();
    QString chosenMac = autoMacs.first();
    if (autoMacs.size() > 1) {
        if (autoMacs.contains(lastMac)) {
            chosenMac = lastMac;
        }
        // Warn once — notifyAutoConnectAmbiguous emits the signal; the
        // MainWindow lambda wired in buildUI surfaces it as a status-bar message.
        m_radioModel->notifyAutoConnectAmbiguous(autoMacs.size(), chosenMac);
    } else if (chosenMac != lastMac && !lastMac.isEmpty()) {
        // Single autoConnect entry but it's not the most recently connected.
        // Still proceed — the user may have switched their autoConnect flag.
    }

    const auto saved = s.savedRadio(chosenMac);
    if (!saved.has_value()) {
        return;  // Shouldn't happen — entry disappeared between the two reads.
    }

    qCInfo(lcConnection) << "Auto-reconnect: attempting" << chosenMac
                         << "at" << saved->info.address.toString()
                         << "(pinToMac=" << saved->pinToMac << ")";

    if (saved->pinToMac) {
        // Use Fast profile — ~480ms per NIC, shorter than the SafeDefault
        // that the user's manual Start Discovery uses.
        RadioDiscovery* disc = m_radioModel->discovery();
        disc->setProfile(DiscoveryProfile::Fast);
        m_autoReconnectInProgress = true;

        // Arm the RadioModel flag so RadioConnection::connectFailed (which fires
        // if the radio replies but fails the handshake) is forwarded as
        // RadioModel::autoConnectFailed. This covers the pinToMac discovery-found
        // but connect-failed path; the timeout path below handles unreachable.
        m_radioModel->setAutoConnectInProgress(true, chosenMac);

        // Listen for a radio that matches our chosen MAC
        QMetaObject::Connection* connPtr = new QMetaObject::Connection;
        *connPtr = connect(disc, &RadioDiscovery::radioDiscovered,
            this, [this, chosenMac, connPtr](const RadioInfo& found) {
            if (found.macAddress != chosenMac) {
                return;
            }
            if (m_radioModel->isConnected()) {
                return; // User beat us to it
            }
            qCInfo(lcConnection) << "Auto-reconnect: MAC found —"
                                 << found.displayName()
                                 << found.address.toString();
            QObject::disconnect(*connPtr);
            delete connPtr;
            m_autoReconnectInProgress = false;
            // Note: do NOT disarm m_radioModel->setAutoConnectInProgress here —
            // connectToRadio runs asynchronously and we want connectFailed to
            // still be forwarded if the handshake itself fails. RadioModel's
            // onConnectionStateChanged(Connected) disarms on success;
            // wireConnectionSignals' connectFailed handler disarms on failure.
            RadioInfo ri = found;
            HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
            if (mo != HPSDRModel::FIRST) {
                ri.modelOverride = mo;
            }
            m_radioModel->connectToRadio(ri);
        });

        // Kick off the Fast-profile discovery
        disc->startDiscovery();

        // 3-second hard timeout — if the MAC never appears, the radio is
        // unreachable on this network. Open the panel + post a status message.
        QTimer::singleShot(3000, this, [this, chosenMac, connPtr]() {
            if (!m_autoReconnectInProgress) {
                // Already connected (discovery lambda cleaned up) — nothing to do.
                return;
            }
            qCInfo(lcConnection) << "Auto-reconnect: 3-second timeout — radio not found";
            // Disconnect the listener so it doesn't fire on later scans
            QObject::disconnect(*connPtr);
            delete connPtr;
            m_autoReconnectInProgress = false;
            // Disarm RadioModel flag — the Timeout path is surfaced here directly
            // (not via connectFailed, which only fires after a reply is received).
            m_radioModel->setAutoConnectInProgress(false);
            // Stop the discovery pass we started; restore SafeDefault profile
            // so the user's next manual scan uses the full timing.
            m_radioModel->discovery()->stopDiscovery();
            m_radioModel->discovery()->setProfile(DiscoveryProfile::SafeDefault);
            // Phase 3Q Task 10: surface the failure — open panel + toast.
            const QString name = AppSettings::instance()
                .savedRadio(chosenMac)
                .value_or(SavedRadio{})
                .info.name;
            const QString displayName = name.isEmpty() ? chosenMac : name;
            showConnectionPanel();
            if (m_connectionPanel) {
                m_connectionPanel->highlightMac(chosenMac);
            }
            showToast(
                QStringLiteral("Auto-connect target %1 isn't reachable from this network. "
                               "Pick a different radio or update the address.")
                    .arg(displayName),
                ToastSeverity::Warning, 8000);
        });
    } else {
        // No MAC pinning — direct connect to saved IP address.
        // Arm m_autoConnectInProgress so that RadioConnection::connectFailed
        // is forwarded as RadioModel::autoConnectFailed → MainWindow lambdas.
        if (!m_radioModel->isConnected()) {
            m_radioModel->setAutoConnectInProgress(true, chosenMac);
            // Load persisted model override for auto-reconnect (Phase 3I-RP)
            RadioInfo ri = saved->info;
            HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
            if (mo != HPSDRModel::FIRST) {
                ri.modelOverride = mo;
            }
            m_radioModel->connectToRadio(ri);
        }
    }
}

// =============================================================================
// Phase 3O Sub-Phase 11 Task 11b — VAX first-run / rescan hook
// =============================================================================
//
// Called once from the constructor via QTimer::singleShot(0, ...). Decides
// whether to show the VaxFirstRunDialog based on audio/FirstRunComplete
// and a SHA-256 diff of the current detected-cable set against the stored
// audio/LastDetectedCables fingerprint. The fingerprint is refreshed on
// every launch regardless of whether the dialog shows, so uninstall +
// reinstall of the same cable doesn't flag it as "new" forever.
//
// NereusSDR-original; no Thetis equivalent.
void MainWindow::checkVaxFirstRun()
{
    auto& s = AppSettings::instance();
    const bool firstRunDone =
        (s.value(QStringLiteral("audio/FirstRunComplete"),
                 QStringLiteral("False")).toString() == QStringLiteral("True"));

    // Platform-specific scan — see detectedForFirstRun() in the anonymous
    // namespace at the top of this file for the platform split rationale.
    const QVector<DetectedCable> detected = detectedForFirstRun();

    // Always refresh the stored fingerprint so a cable being removed +
    // later reinstalled doesn't permanently re-flag itself as "new".
    const QString newCsv = VirtualCableDetector::fingerprintCsv(detected);
    const QString lastCsv = s.value(QStringLiteral("audio/LastDetectedCables"),
                                    QString()).toString();
    s.setValue(QStringLiteral("audio/LastDetectedCables"), newCsv);
    s.save();

    FirstRunScenario scenario;
    QVector<DetectedCable> payload;

    if (!firstRunDone) {
#if defined(Q_OS_WIN)
        scenario = detected.isEmpty() ? FirstRunScenario::WindowsNoCables
                                       : FirstRunScenario::WindowsCablesFound;
#elif defined(Q_OS_MAC)
        scenario = FirstRunScenario::MacNative;
#else
        scenario = FirstRunScenario::LinuxNative;
#endif
        payload = detected;
    } else {
        // First-run already complete — only pop the dialog if NEW cables
        // have appeared since the last launch.
        const auto fresh = VirtualCableDetector::diffNewCables(detected, lastCsv);
        if (fresh.isEmpty()) {
            return;
        }
        scenario = FirstRunScenario::RescanNewCables;
        payload = fresh;
    }

    auto* dlg = new VaxFirstRunDialog(scenario, payload, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // "Apply suggested" / "Apply to VAX 3 & 4" — user accepted the
    // recommended bindings. Log-but-ignore any AudioEngine wiring failure;
    // the design interview explicitly settled that we still mark the
    // first-run complete so the user isn't re-ambushed on next launch.
    connect(dlg, &VaxFirstRunDialog::applySuggested, this,
            [this](const QVector<QPair<int, QString>>& bindings) {
        auto* engine = m_radioModel->audioEngine();
        if (!engine) {
            qCWarning(lcAudio)
                << "VAX first-run: applySuggested with no AudioEngine; "
                   "bindings dropped" << bindings.size();
            return;
        }

        // Remap dialog-suggested bindings onto the first VAX slots
        // whose audio/Vax<ch>/DeviceName is unset. VaxFirstRunDialog::
        // computeSuggestedBindings always numbers its payload starting
        // at VAX 1 regardless of scenario, with an explicit comment
        // that MainWindow is responsible for skipping slots the user
        // has already assigned. Applying the dialog's channel numbers
        // verbatim — the previous revision — clobbered existing slot-1
        // ..N mappings under FirstRunScenario::RescanNewCables. We
        // apply the same rule unconditionally since it is a no-op for
        // WindowsCablesFound (all four DeviceName keys are empty on a
        // fresh install, so remap resolves to the same 1..N order).
        //
        auto& settings = AppSettings::instance();
        int slot = 1;
        for (const auto& b : bindings) {
            while (slot <= 4) {
                const QString key = QStringLiteral("audio/Vax%1/DeviceName")
                                        .arg(slot);
                if (settings.value(key, QString()).toString().isEmpty()) {
                    break;
                }
                ++slot;
            }
            if (slot > 4) {
                qCWarning(lcAudio)
                    << "VAX first-run: no unassigned slots remain;"
                    << "dropping cable" << b.second;
                break;
            }
            AudioDeviceConfig cfg;
            cfg.deviceName = b.second;
            engine->setVaxConfig(slot, cfg);
            engine->setVaxEnabled(slot, true);
            ++slot;
        }
    });

    // Sub-Phase 12: wire "Customize…" / "Why do I need this?" → Setup → VAX.
    // Opens (or raises) the Setup dialog and navigates to Audio → VAX.
    connect(dlg, &VaxFirstRunDialog::openSetupAudioPage, this,
            [this](const QString& pageLabel) {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(pageLabel);
        dialog->show();
    });

    connect(dlg, &VaxFirstRunDialog::openInstallUrl, this,
            [](const QString& url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    // Persist audio/FirstRunComplete on Accepted only (Apply / Skip /
    // Got-it). Rejected covers Escape, window-close, and Customize — none
    // of those should silence the dialog on next launch.
    connect(dlg, &QDialog::finished, this, [](int result) {
        if (result == QDialog::Accepted) {
            auto& settings = AppSettings::instance();
            settings.setValue(QStringLiteral("audio/FirstRunComplete"),
                              QStringLiteral("True"));
            settings.save();
        }
    });

    dlg->show();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Mark shutting-down BEFORE anything else so the "auto-open
    // ConnectionPanel on Disconnect" slot below doesn't re-trigger
    // discovery via ConnectionPanel's ctor while teardown runs.
    m_shuttingDown = true;

    // Force-run any pending coalesced slice save BEFORE we tear anything
    // down. The 500 ms debounce in RadioModel::scheduleSettingsSave can't
    // fire while this handler runs synchronously (event loop blocked on
    // QThread::wait below); without this flush the user's last AF / step /
    // freq / lock / RIT change is silently dropped on close.
    m_radioModel->flushPendingSettingsSave();

    // Stop discovery to prevent new signals during shutdown
    m_radioModel->discovery()->stopDiscovery();

    // Stop FFT thread
    if (m_fftThread && m_fftThread->isRunning()) {
        m_fftThread->quit();
        m_fftThread->wait(2000);
    }

    // Save display settings before shutdown
    if (m_panStack) {
        for (PanadapterApplet* applet : m_panStack->allApplets()) {
            if (applet && applet->spectrumWidget()) {
                applet->spectrumWidget()->saveSettings();
            }
        }
    }

    // Tear down connection (sends stop command, closes sockets, joins thread)
    m_radioModel->disconnectFromRadio();

    // Save container layout
    if (m_containerManager) {
        m_containerManager->saveState();
    }

    // Phase 3F Sub-Epic D Task 15: persist pan layout id + per-splitter
    // sizes. PanadapterStack::saveSplitterState writes PanLayoutId +
    // PanLayoutSplitter_* keys to AppSettings; the matching restore runs
    // during MainWindow init.
    if (m_panStack) {
        m_panStack->saveSplitterState();
    }

    // Issue #206 — persist window geometry + maximized/fullscreen
    // state. Captured BEFORE close so the saved blob reflects the
    // user-visible state, not Qt's mid-teardown geometry.
    saveMainWindowGeometry();

    AppSettings::instance().save();
    event->accept();

    // Ask Qt for an orderly exit from the event loop. Previously called
    // std::exit(0) which runs C++ static destructors before Qt's thread
    // cleanup — that caused QThreadStoragePrivate::finish to fire a qWarning
    // against a destructed QRegularExpression in the PII-redaction message
    // handler, segfaulting every close (~100 diagnostic reports in one day).
    QCoreApplication::quit();
}

// =============================================================================
// Phase 3G-14: AI-Assisted Issue Reporter
// Ported from AetherSDR TitleBar::showFeatureRequestDialog() /
// showFeatureRequestDialogImpl()
// =============================================================================

void MainWindow::showFeatureRequestDialog()
{
    // Version check gate — warn if not on latest release before filing
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/boydsoftprez/NereusSDR/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NereusSDR"));
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString latest = doc.object().value(QStringLiteral("tag_name")).toString();
            if (latest.startsWith(QLatin1Char('v'))) {
                latest = latest.mid(1);
            }
            QVersionNumber latestVer = QVersionNumber::fromString(latest);
            QVersionNumber currentVer = QVersionNumber::fromString(
                QCoreApplication::applicationVersion());
            if (!latestVer.isNull() && currentVer < latestVer) {
                auto answer = QMessageBox::warning(this,
                    QStringLiteral("Outdated Version"),
                    QStringLiteral(
                        "<p>You are running <b>v%1</b> but <b>v%2</b> is available.</p>"
                        "<p>Your issue may already be fixed in the latest release. "
                        "Please update before filing a bug report.</p>"
                        "<p>Continue anyway?</p>")
                        .arg(QCoreApplication::applicationVersion(), latest),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes) {
                    return;
                }
            }
        }
        // Proceed to show the issue dialog
        showFeatureRequestDialogImpl();
    });
}

void MainWindow::showFeatureRequestDialogImpl()
{
    static const QString kPrompt = QStringLiteral(
        "IMPORTANT — before doing anything else, fetch the complete list of open\n"
        "issues by reading pages sequentially until you get fewer than 100 results:\n"
        "  Page 1: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=1\n"
        "  Page 2: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=2\n"
        "  ... continue until a page returns fewer than 100 issues.\n"
        "Do NOT rely on cached or training data for the issue list.\n\n"
        "Also fetch CLAUDE.md fresh (do not use cached versions):\n"
        "  https://raw.githubusercontent.com/boydsoftprez/NereusSDR/main/CLAUDE.md\n\n"
        "I want to report an issue or request a feature for NereusSDR, a cross-platform\n"
        "Qt6/C++20 SDR console for OpenHPSDR radios (ANAN, Hermes Lite 2, etc.). It uses\n"
        "the OpenHPSDR Protocol 1 and Protocol 2 over UDP, with client-side DSP via WDSP.\n\n"
        "DUPLICATE CHECK — this is mandatory. Search the fetched issue list for keywords\n"
        "related to my description below. Check titles AND bodies. If you find an existing\n"
        "issue that covers the same thing, STOP and tell me:\n"
        "  > Duplicate found: #<number> — <title>\n"
        "  > I recommend adding a +1 reaction and a comment describing your use case.\n"
        "Do NOT write a new issue if a duplicate exists.\n\n"
        "If no duplicate exists, determine whether my description is a BUG REPORT or a\n"
        "FEATURE REQUEST, then write a GitHub issue using the appropriate format below.\n"
        "Use GitHub-flavored Markdown formatting (headers, code blocks, bullet points).\n\n"
        "FOR FEATURE REQUESTS include:\n"
        "1. A clear, concise title (imperative mood)\n"
        "2. ## What — what the feature does from the user's perspective\n"
        "3. ## Why — what problem it solves\n"
        "4. ## How Other Clients Do It — how Thetis, PowerSDR, SparkSDR, etc. handle this\n"
        "5. ## Suggested Behavior — specific UX: what the user clicks, sees, what happens.\n"
        "   Reference NereusSDR UI elements (AppletPanel, VfoWidget, RxApplet, SetupDialog, etc.)\n"
        "6. ## Protocol Hints — relevant OpenHPSDR commands, or \"Unknown — needs research\"\n"
        "7. ## Acceptance Criteria — 3-5 bullet points defining done vs not-done\n\n"
        "FOR BUG REPORTS include:\n"
        "1. A clear title describing the broken behavior\n"
        "2. ## What happened — describe the incorrect behavior\n"
        "3. ## What I expected — describe the correct behavior\n"
        "4. ## Steps to reproduce — numbered steps to trigger the bug\n"
        "5. ## Environment — OS, radio model, protocol version, firmware version if relevant\n"
        "6. ## Suggested fix — if you have an idea what's wrong, describe it\n\n"
        "Suggest appropriate labels from: enhancement, bug, documentation,\n"
        "help wanted, good first issue, question\n\n"
        "Here is my idea or bug report:\n\n"
        "[Describe your feature or bug here in plain English]");

    // Reuse existing dialog if still open
    static QPointer<QDialog> sDlg;
    if (sDlg) {
        sDlg->raise();
        sDlg->activateWindow();
        return;
    }

    auto* dlg = new QDialog(this);
    sDlg = dlg;
    dlg->setWindowTitle(QStringLiteral("AI-Assisted Issue Reporter"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(QStringLiteral("QDialog { background: #0f0f1a; }"));
    dlg->setMinimumWidth(620);

    auto* vbox = new QVBoxLayout(dlg);
    vbox->setSpacing(8);
    vbox->setContentsMargins(16, 16, 16, 16);

    auto* header = new QLabel(QStringLiteral(
        "<h3 style='color:#c8d8e8;'>AI-Assisted Issue Reporter</h3>"
        "<p style='color:#8090a0;'>Use any AI assistant to write a detailed bug report or feature request.</p>"
        "<ol style='color:#c8d8e8;'>"
        "<li><b>Choose your AI</b> below — prompt is copied to your clipboard</li>"
        "<li><b>Paste the prompt</b> into the AI chat</li>"
        "<li><b>Describe your idea</b> — edit the [bracketed] section</li>"
        "<li><b>Copy the AI's output</b> and click <b>Submit Your Idea</b></li>"
        "</ol>"));
    header->setWordWrap(true);
    vbox->addWidget(header);

    // Status label — shows after provider selected
    auto* statusLabel = new QLabel;
    statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #20c060; font-size: 11px; font-weight: bold; }"));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->hide();
    vbox->addWidget(statusLabel);

    // AI provider buttons
    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; font-weight: bold; "
        "padding: 6px 12px; }"
        "QPushButton:hover { background: #203040; }");

    auto* btnRow1 = new QHBoxLayout;
    struct Provider { const char* name; const char* url; };
    static constexpr Provider providers[] = {
        {"Claude",     "https://claude.ai/new"},
        {"ChatGPT",    "https://chat.openai.com/"},
        {"Gemini",     "https://gemini.google.com/"},
        {"Grok",       "https://grok.x.ai/"},
        {"Perplexity", "https://www.perplexity.ai/"},
    };
    for (const auto& p : providers) {
        auto* btn = new QPushButton(QString::fromUtf8(p.name), dlg);
        btn->setStyleSheet(btnStyle);
        btn->setAutoDefault(false);
        QString url = QString::fromUtf8(p.url);
        connect(btn, &QPushButton::clicked, dlg, [url, statusLabel] {
            QApplication::clipboard()->setText(kPrompt);
            QDesktopServices::openUrl(QUrl(url));
            statusLabel->setText(QStringLiteral(
                "Prompt copied to clipboard — paste into the AI, "
                "then come back and click Submit Your Idea"));
            statusLabel->show();
        });
        btnRow1->addWidget(btn);
    }
    vbox->addLayout(btnRow1);

    vbox->addSpacing(8);

    // Submit / Report / Close
    auto* btnRow2 = new QHBoxLayout;

    auto* submitBtn = new QPushButton(QStringLiteral("Submit Your Idea"), dlg);
    submitBtn->setAutoDefault(false);
    submitBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #00b4d8; color: #0f0f1a; font-weight: bold; "
        "border-radius: 4px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #00c8f0; }"));
    connect(submitBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=feature_request.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(submitBtn);

    auto* bugBtn = new QPushButton(QStringLiteral("Report a Bug"), dlg);
    bugBtn->setAutoDefault(false);
    bugBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #cc4040; color: #ffffff; font-weight: bold; "
        "border-radius: 4px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #dd5050; }"));
    connect(bugBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=bug_report.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(bugBtn);

    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    closeBtn->setAutoDefault(false);
    closeBtn->setStyleSheet(btnStyle);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    btnRow2->addWidget(closeBtn);
    vbox->addLayout(btnRow2);

    // Copy prompt to clipboard on first open
    QApplication::clipboard()->setText(kPrompt);

    dlg->show();
}

} // namespace NereusSDR
