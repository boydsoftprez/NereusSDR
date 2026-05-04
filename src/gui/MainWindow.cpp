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
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "widgets/VfoWidget.h"
#include "widgets/RxDashboard.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"  // H.2: setTxChannel wiring
#include "core/ReceiverManager.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/FFTEngine.h"
#include "core/NbFamily.h"
#include "core/ClarityController.h"
#include "core/StepAttenuatorController.h"
#include "core/MoxController.h"  // 3M-1a G.1: F.2 connect (hardwareFlipped → onMoxHardwareFlipped)
#include "core/NoiseFloorTracker.h"
#include "core/BoardCapabilities.h"
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
#include "applets/RxApplet.h"
#include "applets/TxApplet.h"
#include "applets/TxEqDialog.h"
#include "applets/PhoneCwApplet.h"
#include "applets/EqApplet.h"
#include "applets/VaxApplet.h"
#include "applets/DigitalApplet.h"
#include "applets/PureSignalApplet.h"
#include "applets/DiversityApplet.h"
#include "applets/CwxApplet.h"
#include "applets/DvkApplet.h"
#include "applets/CatApplet.h"
#include "applets/TunerApplet.h"
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

#include <QApplication>
#include <QSlider>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QMenuBar>
#include <QMenu>
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
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QPointer>

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
    // the deviceName; AudioEngine::makeBus fills in sensible defaults
    // (sample rate 48 kHz stereo, default buffer) per AudioEngine.h:165.
    connect(m_titleBar->masterOutput(), &MasterOutputWidget::outputDeviceChanged,
            this, [this](const QString& name) {
        AudioDeviceConfig cfg;
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
        if (QStatusBar* sb = statusBar()) {
            sb->showMessage(reason, 3000);
        }
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
        statusBar()->showMessage(
            QStringLiteral("Auto-connect target %1 %2. Pick a different radio or update the address.")
                .arg(name, reasonText),
            8000);
    });

    // autoConnectAmbiguous — multiple radios have AutoConnect = true.
    // The most-recently-connected MAC was chosen; post a 6-second advisory
    // pointing the user at Manage Radios for cleanup.
    connect(m_radioModel, &RadioModel::autoConnectAmbiguous,
            this, [this](int count, const QString& chosenMac) {
        const auto saved = AppSettings::instance().savedRadio(chosenMac);
        const QString name = (saved.has_value() && !saved->info.name.isEmpty())
            ? saved->info.name : chosenMac;
        statusBar()->showMessage(
            QStringLiteral("%1 radios marked Auto-connect on launch. Using %2 (most recent). "
                           "Adjust in Manage Radios.")
                .arg(count).arg(name),
            6000);
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
        if (m_radioModel) {
            m_radioModel->flushPendingSettingsSave();
        }
        if (m_containerManager) {
            m_containerManager->saveState();
        }
        AppSettings::instance().save();
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUI()
{
    // Issue #100: suffix the title with the active profile so users
    // running two instances against different radios can tell the
    // windows apart.
    const QString profile = AppSettings::profileOverride();
    if (profile.isEmpty()) {
        setWindowTitle(QStringLiteral("NereusSDR %1").arg(NEREUSSDR_VERSION));
    } else {
        setWindowTitle(QStringLiteral("NereusSDR %1 [%2]")
                           .arg(NEREUSSDR_VERSION, profile));
    }
    setMinimumSize(800, 600);
    resize(1280, 800);

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

    m_spectrumWidget = new SpectrumWidget(spectrumPane);
    m_spectrumWidget->loadSettings();
    m_spectrumWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_spectrumWidget, 1);

    // Left overlay panel (SpectrumOverlayPanel) — child of spectrum widget
    m_overlayPanel = new SpectrumOverlayPanel(m_spectrumWidget);
    m_overlayPanel->move(4, 4);
    m_overlayPanel->show();

    // Phase 3O Sub-Phase 9 Task 9.2c — bind the overlay's VAX Ch combo to
    // the RadioModel. The combo stays disabled until slice 0 exists
    // (setRadioModel listens to sliceAdded), then flips live.
    m_overlayPanel->setRadioModel(m_radioModel);

    // Phase 3P-I-a T18 — push board caps into the overlay's antenna
    // combos on connect and on every radio swap. Hides both RX/TX
    // rows on HL2/Atlas and reseeds from slice 0's rxAntenna/txAntenna
    // so persisted per-band state is visible in the combo label.
    m_overlayPanel->setBoardCapabilities(m_radioModel->boardCapabilities());
    connect(m_radioModel, &RadioModel::currentRadioChanged, m_overlayPanel,
            [this]() {
        m_overlayPanel->setBoardCapabilities(m_radioModel->boardCapabilities());
    });

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
        m_spectrumWidget->setFrequencyRange(m_spectrumWidget->centerFrequency(), bwHz);
        emit m_spectrumWidget->bandwidthChangeRequested(bwHz);
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

    // Phase 3Q Sub-PR-6 (F.1): bind RxDashboard to slice(0) once it exists.
    // buildStatusBar() runs before connectToRadio() so slices().isEmpty() at
    // construction time; defer binding to sliceAdded.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index == 0 && m_rxDashboard) {
            m_rxDashboard->bindSlice(m_radioModel->slices().at(0));
        }
    });

    // Create FFTEngine on a worker thread (spectrum thread from architecture).
    // Sample rate starts at P2 default (768k); RadioModel::wireSampleRateChanged
    // updates it to the actual wire rate on each connect (P1=192k, P2=768k).
    m_fftEngine = new FFTEngine(0);  // receiver 0
    m_fftEngine->setSampleRate(768000.0);
    connect(m_radioModel, &RadioModel::wireSampleRateChanged,
            this, [this](double rateHz) {
        if (m_fftEngine) {
            QMetaObject::invokeMethod(m_fftEngine, [engine = m_fftEngine, rateHz]() {
                engine->setSampleRate(rateHz);
            });
        }
        if (m_spectrumWidget) {
            m_spectrumWidget->setSampleRate(rateHz);
            // Phase 3G-12: preserve the user's current zoom level across
            // sample rate changes. Only reset the visible span if the
            // current bandwidth would now exceed the new DDC sample rate
            // (in which case we clamp to full-span).
            const double freq = m_spectrumWidget->centerFrequency();
            const double currentBw = m_spectrumWidget->bandwidth();
            const double clampedBw = (currentBw > rateHz) ? rateHz : currentBw;
            m_spectrumWidget->setFrequencyRange(freq, clampedBw);
        }
    });
    m_fftEngine->setFftSize(4096);
    m_fftEngine->setOutputFps(30);

    m_fftThread = new QThread(this);
    m_fftThread->setObjectName(QStringLiteral("SpectrumThread"));
    m_fftEngine->moveToThread(m_fftThread);

    // Clean up FFTEngine when thread finishes
    connect(m_fftThread, &QThread::finished, m_fftEngine, &QObject::deleteLater);

    // Wire: RadioModel raw I/Q → FFTEngine (auto-queued: main → spectrum thread)
    connect(m_radioModel, &RadioModel::rawIqData,
            m_fftEngine, &FFTEngine::feedIQ);

    // Wire: FFTEngine FFT bins → SpectrumWidget (auto-queued: spectrum → main thread)
    connect(m_fftEngine, &FFTEngine::fftReady,
            m_spectrumWidget, &SpectrumWidget::updateSpectrum);

    // Phase 3G-8: expose view hooks on RadioModel so Display setup pages can
    // reach the renderer / FFT engine without depending on MainWindow.
    m_radioModel->setSpectrumWidget(m_spectrumWidget);
    m_radioModel->setFftEngine(m_fftEngine);

    // Sub-epic E: flush the rewind ring buffer when the radio disconnects so
    // a new session starts with a clean history. AetherSDR's clearDisplay()
    // did this implicitly; NereusSDR has no equivalent single-call reset, so
    // we plumb the connection-state signal through here. See
    // docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md task 4.
    connect(m_radioModel, &RadioModel::connectionStateChanged, m_spectrumWidget,
            [this]() {
        if (!m_radioModel->isConnected() && m_spectrumWidget) {
            m_spectrumWidget->clearWaterfallHistory();
        }
    });

    // Phase 3Q-8: clicking the spectrum while disconnected opens ConnectionPanel.
    connect(m_spectrumWidget, &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::showConnectionPanel);

    // Wire BandPlanManager → SpectrumWidget so the bandplan strip renders on launch.
    m_spectrumWidget->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

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
        if (m_spectrumWidget) {
            connect(mox, &MoxController::moxStateChanged,
                    m_spectrumWidget, &SpectrumWidget::setMoxOverlay,
                    Qt::QueuedConnection);
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
        m_spectrumWidget->setClarityActive(clarityOn);
    }

    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main)
    connect(m_fftEngine, &FFTEngine::fftReady,
            m_clarityController, [this](int /*rxId*/, const QVector<float>& binsDbm) {
        m_clarityController->feedBins(binsDbm);
    });

    // ── NoiseFloorTracker for Auto AGC-T ────────────────────────────────
    auto* nfTracker = new NoiseFloorTracker;
    m_radioModel->setNoiseFloorTracker(nfTracker);

    connect(m_fftEngine, &FFTEngine::fftReady,
            this, [nfTracker](int /*rxId*/, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nfTracker->feed(binsDbm, kFrameIntervalMs);
    });

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
    if (m_spectrumWidget) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::filterChanged,
                m_spectrumWidget, &SpectrumWidget::setTxFilterRange);

        // Initial sync from current TransmitModel state.
        const auto& txModel = m_radioModel->transmitModel();
        m_spectrumWidget->setTxFilterRange(txModel.filterLow(), txModel.filterHigh());
    }

    // Clarity → SpectrumWidget threshold update + clarityActive flag
    connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
            m_spectrumWidget, [this](float low, float high) {
        m_spectrumWidget->setClarityActive(true);
        m_spectrumWidget->setWfLowThreshold(low);
        m_spectrumWidget->setWfHighThreshold(high);
    });

    // When Clarity pauses or is disabled, let legacy AGC resume.
    connect(m_clarityController, &ClarityController::pausedChanged,
            m_spectrumWidget, [this](bool paused) {
        if (paused) {
            m_spectrumWidget->setClarityActive(false);
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
                m_spectrumWidget, &SpectrumWidget::setWfColorGain);
        connect(m_overlayPanel, &SpectrumOverlayPanel::wfBlackLevelChanged,
                m_spectrumWidget, &SpectrumWidget::setWfBlackLevel);
        connect(m_overlayPanel, &SpectrumOverlayPanel::colorSchemeChanged,
                m_spectrumWidget, [this](int idx) {
            // colorSchemeChanged carries a raw combo index (int); setWfColorScheme
            // takes the WfColorScheme enum — adapt with a bounds-checked cast.
            const int schemeCount = static_cast<int>(WfColorScheme::Count);
            m_spectrumWidget->setWfColorScheme(
                static_cast<WfColorScheme>(qBound(0, idx, schemeCount - 1)));
        });

        // B8 Task 21: wire Cursor Freq toggle to SpectrumWidget visibility guard.
        connect(m_overlayPanel, &SpectrumOverlayPanel::cursorFreqVisibleChanged,
                m_spectrumWidget, &SpectrumWidget::setCursorFreqVisible);

        // B8 Task 22: wire Fill Color button to SpectrumWidget::setFillColor.
        connect(m_overlayPanel, &SpectrumOverlayPanel::fillColorChanged,
                m_spectrumWidget, &SpectrumWidget::setFillColor);

        // B8 fix-up: wire Fill Alpha slider to SpectrumWidget::setFillAlpha.
        // The slider emitted fillAlphaChanged but had no connect — opacity
        // never reached the renderer.
        connect(m_overlayPanel, &SpectrumOverlayPanel::fillAlphaChanged,
                m_spectrumWidget, &SpectrumWidget::setFillAlpha);

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

    // Wire: zoom changes → adjust FFT size for appropriate bin resolution
    // Target: ~500-1000 bins across the visible bandwidth for good detail
    connect(m_spectrumWidget, &SpectrumWidget::bandwidthChangeRequested,
            this, [this](double bwHz) {
        // Pick FFT size so bin_width ≈ bw / 1000 (aim for ~1000 bins across display)
        // bin_width = sampleRate / fftSize → fftSize = sampleRate / bin_width
        double sampleRate = m_spectrumWidget->sampleRate();
        int targetBins = 1000;
        int desiredSize = static_cast<int>(sampleRate * targetBins / bwHz);
        // Round up to next power of 2, clamp to valid range
        int fftSize = 1024;
        while (fftSize < desiredSize && fftSize < 65536) {
            fftSize *= 2;
        }
        fftSize = std::clamp(fftSize, 1024, 65536);
        m_fftEngine->setFftSize(fftSize);
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
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
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

    // H.2 (Phase 3M-1a): wire MoxController::moxStateChanged → MeterPoller::setInTx.
    // Switches the poll set between RX meters (TX off) and TX meters (TX on).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter dispatch.
    // Qt::QueuedConnection: ensures the flip happens at the start of the next
    // event loop tick rather than mid-poll, matching Thetis's timer dispatch.
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::moxStateChanged,
                m_meterPoller, &MeterPoller::setInTx,
                Qt::QueuedConnection);

        // ── Phase 3M-1b K.2: MOX rejection → status-bar toast ───────────────
        // moxRejected fires when BandPlanGuard::checkMoxAllowed() rejects a
        // setMox(true) request (wrong mode, out-of-band freq, cross-band TX).
        // showMessage(reason, 3000) presents the rejection reason for 3 seconds
        // in the Qt status bar, matching the bandClickIgnored toast pattern
        // wired at MainWindow.cpp:418.  The toast is transient — it clears
        // automatically and does not affect status-bar layout or persistence.
        connect(mox, &MoxController::moxRejected,
                this, [this](const QString& reason) {
            if (QStatusBar* sb = statusBar()) {
                sb->showMessage(reason, 3000);
            }
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

    // Build an AppletPanelWidget: MeterWidget on top, then all applets below.
    // This is a single scrollable content widget per the v2 plan.
    m_appletPanel = new AppletPanelWidget();
    auto* panel = m_appletPanel;
    // Fixed header: MeterWidget stays visible, never scrolls.
    // Height scales dynamically with width. 1.3:1 gives the S-Meter arc
    // enough vertical room to not look squished.
    panel->setHeaderWidget(m_meterWidget, QStringLiteral("Meters"), 1.3f);

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
                    const auto slices = m_radioModel->slices();
                    if (index >= 0 && index < slices.size()) {
                        subscribeToSlice(slices[index]);
                    }
                });
    }

    // PhoneCwApplet — Phone + CW pages, NYI
    m_phoneCwApplet = new PhoneCwApplet(m_radioModel, nullptr);
    panel->addApplet(m_phoneCwApplet);

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

    // Ghost applets: constructed but not added to the panel or the Containers menu
    // until their feature phases ship. Uncomment the construction + addContainerToggle
    // call (in buildMenuBar) together when the feature lands.
    //
    // m_digitalApplet    = new DigitalApplet(m_radioModel, nullptr);    // TODO 3-VAX
    // m_pureSignalApplet = new PureSignalApplet(m_radioModel, nullptr); // TODO 3M-4 (PureSignal)
    // m_diversityApplet  = new DiversityApplet(m_radioModel, nullptr);  // TODO 3F (multi-RX)
    // m_cwxApplet        = new CwxApplet(m_radioModel, nullptr);        // TODO 3M-2 (CW TX)
    // m_dvkApplet        = new DvkApplet(m_radioModel, nullptr);        // TODO 3M-1 (DVK)
    // m_catApplet        = new CatApplet(m_radioModel, nullptr);        // TODO 3J/3K/3-VAX
    // m_tunerApplet      = new TunerApplet(m_radioModel, nullptr);      // TODO ATU phase

    c0->setContent(panel);
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
    qCDebug(lcContainer) << "Container #0: Meters + RxApplet + TxApplet + PhoneCwApplet + VaxApplet";
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

    {
        QMenu* panLayoutMenu = viewMenu->addMenu(QStringLiteral("Pan &Layout"));
        QActionGroup* layoutGroup = new QActionGroup(this);
        layoutGroup->setExclusive(true);
        const struct { const char* label; } layouts[] = {
            { "&Single" }, { "2 &Vertical" }, { "2 &Horizontal" },
            { "2×&2" }, { "1+2 &Horizontal" }
        };
        bool first = true;
        for (const auto& l : layouts) {
            QAction* a = panLayoutMenu->addAction(QString::fromUtf8(l.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase 3F"));
            if (first) { a->setChecked(true); first = false; }
            layoutGroup->addAction(a);
        }
    }

    {
        QAction* addPanAction = viewMenu->addAction(QStringLiteral("&Add Panadapter"));
        addPanAction->setEnabled(false);
        addPanAction->setToolTip(QStringLiteral("NYI — Phase 3F"));
    }
    {
        QAction* rmPanAction = viewMenu->addAction(QStringLiteral("&Remove Panadapter"));
        rmPanAction->setEnabled(false);
        rmPanAction->setToolTip(QStringLiteral("NYI — Phase 3F"));
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
                if (m_spectrumWidget) {
                    m_spectrumWidget->setBandPlanFontSize(pt);
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
    // ANF writes through RxChannel directly (SliceModel::anfEnabled is a
    // Task 17 follow-up). Cross-surface sync is therefore not yet possible
    // for ANF; the action emits its own toggled() but isn't checked from
    // model state. SNB / APF / BIN go through SliceModel and are synced.
    {
        QAction* anfAction = dspMenu->addAction(QStringLiteral("&ANF"));
        anfAction->setCheckable(true);
        connect(anfAction, &QAction::toggled, this, [this](bool on) {
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) { rxCh->setAnfEnabled(on); }
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
    // connection survives slice teardown/re-create. ANF intentionally
    // omitted — its state lives on RxChannel without a notify signal
    // (SliceModel::anfEnabled is a Task 17 follow-up).
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
        QAction* psAction = dspMenu->addAction(QStringLiteral("&PureSignal..."));
        psAction->setEnabled(false);
        psAction->setToolTip(QStringLiteral("NYI — Phase 3I-4"));
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

    // 12 modes in display order (spec order): LSB, USB, DSB, CWL, CWU, AM,
    // SAM, FM, DIGL, DIGU, DRM, SPEC.
    // Maps to DSPMode enum values from WdspTypes.h.
    // From Thetis dsp.cs DSPMode enum — enum values used directly, not indices.
    const struct { const char* label; DSPMode mode; } modes[] = {
        { "LSB",  DSPMode::LSB  },
        { "USB",  DSPMode::USB  },
        { "DSB",  DSPMode::DSB  },
        { "CWL",  DSPMode::CWL  },
        { "CWU",  DSPMode::CWU  },
        { "AM",   DSPMode::AM   },
        { "SAM",  DSPMode::SAM  },
        { "FM",   DSPMode::FM   },
        { "DIGL", DSPMode::DIGL },
        { "DIGU", DSPMode::DIGU },
        { "DRM",  DSPMode::DRM  },
        { "SPEC", DSPMode::SPEC },
    };

    m_modeActionGroup = new QActionGroup(this);
    m_modeActionGroup->setExclusive(true);

    for (int i = 0; i < 12; ++i) {
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
            };
            for (int i = 0; i < 12; ++i) {
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

    // Dynamic show/hide toggles for optional applets in Container #0.
    // Checked = visible in panel; unchecked = hidden/removed.
    // Currently commented out (ghost applets hidden per
    // docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 6).
    // Re-enable by un-commenting the lambda AND the addContainerToggle calls below,
    // alongside the construction block in buildDefaultContainerLayout().
    //
    // auto addContainerToggle = [&](const QString& name, AppletWidget* applet, bool defaultVisible) {
    //     auto* action = containersMenu->addAction(name);
    //     action->setCheckable(true);
    //     action->setChecked(defaultVisible);
    //     connect(action, &QAction::toggled, this, [this, applet](bool show) {
    //         if (!m_appletPanel) { return; }
    //         if (show) {
    //             m_appletPanel->addApplet(applet);
    //         } else {
    //             m_appletPanel->removeApplet(applet);
    //         }
    //     });
    // };

    // Ghost-applet Containers menu entries — disabled until feature phases ship.
    // Re-enable alongside the construction block in buildDefaultContainerLayout().
    //
    // addContainerToggle(QStringLiteral("Digital / VAC"), m_digitalApplet,    false); // TODO 3-VAX
    // addContainerToggle(QStringLiteral("PureSignal"),    m_pureSignalApplet, false); // TODO 3M-4
    // addContainerToggle(QStringLiteral("Diversity"),     m_diversityApplet,  false); // TODO 3F
    // addContainerToggle(QStringLiteral("CW Keyer"),      m_cwxApplet,        false); // TODO 3M-2
    // addContainerToggle(QStringLiteral("Voice Keyer"),   m_dvkApplet,        false); // TODO 3M-1
    // addContainerToggle(QStringLiteral("CAT / TCI"),     m_catApplet,        false); // TODO 3J/3K
    // addContainerToggle(QStringLiteral("ATU Control"),   m_tunerApplet,      false); // TODO ATU

    // =========================================================================
    // TOOLS
    // =========================================================================
    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    // TX Equalizer — modeless singleton dialog (Phase 3M-3a-i Batch 3 A.1).
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
        QAction* tciAction = toolsMenu->addAction(QStringLiteral("&TCI Server..."));
        tciAction->setEnabled(false);
        tciAction->setToolTip(QStringLiteral("NYI — Phase 3J"));
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

    // +PAN icon (NYI)
    auto* panLabel = new QLabel(QStringLiteral("+PAN"), barWidget);
    panLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    panLabel->setToolTip(QStringLiteral("+PAN (NYI — Phase 3F)"));
    panLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(panLabel);

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

    // TCI — NYI until Phase 3J; kept as static indicator, no live signal
    m_tciIndicator = makeIndicator(QStringLiteral("TCI"), QStringLiteral("Off"));
    hbox->addWidget(m_tciIndicator);
    m_tciSep = makeSep();
    hbox->addWidget(m_tciSep);

    // ── PA / supply voltage (MKII-class boards only) ──────────────────────
    // Earlier revisions of this section showed a "PSU" widget driven by
    // the supply_volts (AIN6) channel. Source-first audit against Thetis
    // [v2.10.3.13] proved that channel is never displayed in Thetis —
    // computeHermesDCVoltage() exists but has zero callers, and the only
    // voltage status indicator (toolStripStatusLabel_Volts) reads
    // _MKIIPAVolts which is convertToVolts(getUserADC0()) — i.e. the PA
    // drain voltage on AIN3. On a G2 / 8000D / 7000DLE the PA drain IS
    // the supply voltage minus a small drop, so this single number
    // covers what the user wants to know.
    //
    // This widget is hidden by default and shown on the first
    // userAdc0Changed signal (which only fires for MKII-class boards
    // per the gate at P2RadioConnection.cpp:2153-2164). Non-MKII
    // boards (Hermes, Atlas) get no voltage display, matching Thetis
    // (HardwareSpecific.HasVolts gate).
    m_paVoltLabel = new MetricLabel(QStringLiteral("PA"),
                                    QStringLiteral("—"), barWidget);
    m_paVoltLabel->setVisible(false);
    hbox->addWidget(m_paVoltLabel);
    m_paVoltLabelSep = makeSep();
    m_paVoltLabelSep->setVisible(false);
    hbox->addWidget(m_paVoltLabelSep);

    // Wire voltage signals: re-bind on every new connection, reset on disconnect.
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState s) {
        if (s != ConnectionState::Connected) {
            const bool wasShown = m_paVoltLabel->isVisible();
            m_paVoltLabel->setVisible(false);
            if (m_paVoltLabelSep) { m_paVoltLabelSep->setVisible(false); }
            if (wasShown) {
                // PA volt widget just disappeared — drops may unwind.
                // force=true: budget hasn't moved but content width has.
                reapplyRightStripDropPriority(/*force=*/true);
            }
        }
        if (auto* conn = m_radioModel->connection()) {
            // conn is a new object on each reconnect — no deduplication needed.
            // Qt::UniqueConnection is not supported for lambda connects anyway.
            connect(conn, &RadioConnection::userAdc0Changed, this,
                    [this](float v) {
                const bool wasHidden = !m_paVoltLabel->isVisible();
                m_paVoltLabel->setValue(QString::asprintf("%.1fV", static_cast<double>(v)));
                m_paVoltLabel->setVisible(true);
                if (m_paVoltLabelSep) { m_paVoltLabelSep->setVisible(true); }
                if (wasHidden) {
                    // PA volt widget just appeared — recompute drops.
                    // force=true: budget hasn't moved but content width has.
                    reapplyRightStripDropPriority(/*force=*/true);
                }
            });
        }
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
    m_cpuTimer->start(1000);

    // Add the full-width bar widget to the status bar
    sb->addWidget(barWidget, 1);
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
}

void MainWindow::wireSliceToSpectrum()
{
    SliceModel* slice = m_radioModel->activeSlice();
    if (!slice || !m_spectrumWidget) {
        return;
    }

    // Set initial spectrum display. Phase 3G-12: preserve the user's
    // persisted zoom level if present. SpectrumWidget::loadSettings()
    // has already read "DisplayBandwidth" from AppSettings into
    // m_bandwidthHz by this point. If the loaded value is sensible
    // (between 10 kHz and the DDC sample rate), keep it; otherwise
    // fall back to the full-span default (768 kHz = sample rate).
    double freq = slice->frequency();
    const double loadedBw = m_spectrumWidget->bandwidth();
    const double initialBw = (loadedBw >= 10000.0 && loadedBw <= 768000.0)
                             ? loadedBw : 768000.0;
    m_spectrumWidget->setFrequencyRange(freq, initialBw);
    m_spectrumWidget->setDdcCenterFrequency(freq);
    m_spectrumWidget->setSampleRate(768000.0);
    m_spectrumWidget->setVfoFrequency(freq);
    m_spectrumWidget->setFilterOffset(slice->filterLow(), slice->filterHigh());
    m_spectrumWidget->setStepSize(slice->stepHz());

    // --- Create floating VFO flag widget (AetherSDR pattern) ---
    VfoWidget* vfo = m_spectrumWidget->addVfoWidget(0);
    m_vfoWidget = vfo;
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
    connect(m_radioModel, &RadioModel::currentRadioChanged, vfo,
            [this, vfo]() {
        vfo->setBoardCapabilities(m_radioModel->boardCapabilities());
        vfo->setHpsdrSku(m_radioModel->hardwareProfile().model);
    });

    // Phase 3P-I-b T9 — VFO BYPS button ↔ AlexController::rxOutOnTx
    connect(vfo, &VfoWidget::rxBypassToggled,
            &m_radioModel->alexControllerMutable(), &AlexController::setRxOutOnTx);
    connect(&m_radioModel->alexController(), &AlexController::rxOutOnTxChanged,
            vfo, &VfoWidget::setRxBypassActive);

    // Stage C2: wire FilterPresetStore so VFO flag filter buttons use user overrides.
    vfo->setFilterPresetStore(m_radioModel->filterPresetStore());

    // --- Slice → spectrum display ---

    // VFO frequency change → move VFO marker
    // In CTUN mode (SmartSDR-style): pan stays fixed, VFO moves within it.
    // In traditional mode: pan follows VFO (auto-scroll handled in setVfoFrequency).
    // Band changes (large jumps) always recenter regardless of mode.
    connect(slice, &SliceModel::frequencyChanged, this, [this, vfo, slice](double freq) {
        if (m_handlingBandJump) {
            return;
        }

        double center = m_spectrumWidget->centerFrequency();
        double halfBw = m_spectrumWidget->bandwidth() / 2.0;
        bool offScreen = (freq < center - halfBw) || (freq > center + halfBw);

        if (!m_spectrumWidget->ctunEnabled() || offScreen) {
            m_handlingBandJump = true;

            bool wasCTUN = m_spectrumWidget->ctunEnabled();
            m_radioModel->receiverManager()->setDdcFrequencyLocked(false);

            m_spectrumWidget->setCenterFrequency(freq);

            int rxIdx = slice->receiverIndex();
            if (rxIdx >= 0) {
                m_radioModel->receiverManager()->forceHardwareFrequency(
                    rxIdx, static_cast<quint64>(freq));
            }
            m_spectrumWidget->setDdcCenterFrequency(freq);

            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(0.0);
            }

            if (wasCTUN) {
                m_radioModel->receiverManager()->setDdcFrequencyLocked(true);
            }

            m_handlingBandJump = false;
        } else {
            // From Thetis radio.rs:1417 — WDSP shift = +(freq - center)
            double shiftHz = freq - center;
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(shiftHz);
            }
        }
        m_spectrumWidget->setVfoFrequency(freq);
        vfo->setFrequency(freq);
    });

    connect(slice, &SliceModel::filterChanged, this, [this, vfo](int low, int high) {
        m_spectrumWidget->setFilterOffset(low, high);
        vfo->setFilter(low, high);
    });

    // Plan 4 D9 (Cluster E): initial TX mode push so the overlay has the right
    // IQ-space sign convention before the first paint.
    if (m_spectrumWidget) {
        m_spectrumWidget->setTxMode(slice->dspMode());
        // Initial XIT offset push + signal wires below so the TX overlay
        // centers on the actual TX frequency (RX VFO + XIT) rather than the
        // RX VFO alone.  Codex review feedback on PR #166.
        const int initialXitOffset = slice->xitEnabled() ? slice->xitHz() : 0;
        m_spectrumWidget->setTxVfoOffsetHz(initialXitOffset);
    }

    // XIT-enabled toggle and XIT-Hz changes both feed the spectrum's TX
    // overlay center.  When enabled flips off, the offset goes to zero;
    // when on, the offset tracks xitHz.
    auto pushXitOffset = [this, slice]() {
        if (!m_spectrumWidget) { return; }
        m_spectrumWidget->setTxVfoOffsetHz(slice->xitEnabled() ? slice->xitHz() : 0);
    };
    connect(slice, &SliceModel::xitEnabledChanged, this,
            [pushXitOffset](bool /*enabled*/) { pushXitOffset(); });
    connect(slice, &SliceModel::xitHzChanged, this,
            [pushXitOffset](int /*hz*/) { pushXitOffset(); });

    connect(slice, &SliceModel::dspModeChanged, this, [this, vfo](DSPMode mode) {
        // Plan 4 D9 (Cluster E): keep TX mode in sync so drawTxFilterOverlay
        // maps audio Hz to the correct IQ-space sideband.
        if (m_spectrumWidget) {
            m_spectrumWidget->setTxMode(mode);
        }
        vfo->setMode(mode);
        // Switch PhoneCwApplet page based on active mode
        if (m_phoneCwApplet) {
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
                    break;
            }
        }
    });

    connect(slice, &SliceModel::agcModeChanged, this, [vfo](AGCMode mode) {
        vfo->setAgcMode(mode);
    });

    connect(slice, &SliceModel::afGainChanged, this, [vfo](int gain) {
        vfo->setAfGain(gain);
    });

    connect(slice, &SliceModel::rfGainChanged, this, [vfo](int gain) {
        vfo->setRfGain(gain);
    });

    connect(slice, &SliceModel::stepHzChanged, this, [this, vfo](int hz) {
        m_spectrumWidget->setStepSize(hz);
        vfo->setStepHz(hz);
    });

    connect(slice, &SliceModel::rxAntennaChanged, this, [vfo](const QString& ant) {
        vfo->setRxAntenna(ant);
    });

    connect(slice, &SliceModel::txAntennaChanged, this, [vfo](const QString& ant) {
        vfo->setTxAntenna(ant);
    });

    // --- SliceModel → VfoWidget: RIT/XIT inbound (S1.8a stubs) ---
    connect(slice, &SliceModel::ritEnabledChanged, this, [vfo](bool on) {
        vfo->setRitEnabled(on);
    });

    connect(slice, &SliceModel::ritHzChanged, this, [vfo](int hz) {
        vfo->setRitHz(hz);
    });

    connect(slice, &SliceModel::xitEnabledChanged, this, [vfo](bool on) {
        vfo->setXitEnabled(on);
    });

    connect(slice, &SliceModel::xitHzChanged, this, [vfo](int hz) {
        vfo->setXitHz(hz);
    });

    // --- SliceModel → VfoWidget: DSP tab inbound (S1.8b) ---
    connect(slice, &SliceModel::nbModeChanged, vfo, &VfoWidget::setNbMode);
    vfo->setNbMode(slice->nbMode());   // initial sync

    // Sub-epic C-1: NR bank sync — VfoWidget::setSlice also connects activeNrChanged
    // via onActiveNrChanged for the full 7-button bank; this redundant connection is
    // removed to avoid double-firing. setSlice handles both initial sync and updates.
    // (Legacy setNr2Enabled call removed here — onActiveNrChanged covers NR2.)

    connect(slice, &SliceModel::snbEnabledChanged, this, [vfo](bool v) {
        vfo->setSnbEnabled(v);
    });

    connect(slice, &SliceModel::apfEnabledChanged, this, [vfo](bool v) {
        vfo->setApfEnabled(v);
    });

    connect(slice, &SliceModel::apfTuneHzChanged, this, [vfo](int hz) {
        vfo->setApfTuneHz(hz);
    });

    // --- VFO flag → slice ---

    connect(vfo, &VfoWidget::frequencyChanged, this, [slice](double hz) {
        slice->setFrequency(hz);
    });

    connect(vfo, &VfoWidget::modeChanged, this, [slice](DSPMode mode) {
        slice->setDspMode(mode);
    });

    connect(vfo, &VfoWidget::filterChanged, this, [slice](int low, int high) {
        slice->setFilter(low, high);
    });

    // Shift+click on a filter preset on the flag — snap TX passband to
    // match the RX preset's audio Hz range (Thetis-style alignment shortcut).
    connect(vfo, &VfoWidget::txFilterMatchRequested, this,
            [this](int audioLow, int audioHigh) {
        m_radioModel->transmitModel().setFilterLow(audioLow);
        m_radioModel->transmitModel().setFilterHigh(audioHigh);
    });

    connect(vfo, &VfoWidget::agcModeChanged, this, [slice](AGCMode mode) {
        slice->setAgcMode(mode);
    });

    connect(vfo, &VfoWidget::afGainChanged, this, [slice](int gain) {
        slice->setAfGain(gain);
    });

    connect(vfo, &VfoWidget::rfGainChanged, this, [slice](int gain) {
        slice->setRfGain(gain);
    });

    connect(vfo, &VfoWidget::rxAntennaChanged, this, [slice](const QString& ant) {
        slice->setRxAntenna(ant);
    });

    connect(vfo, &VfoWidget::txAntennaChanged, this, [slice](const QString& ant) {
        slice->setTxAntenna(ant);
    });

    // NB cycling — nbModeCycled fires on user click; cycle the mode through
    // Off → NB → NB2 → Off via SliceModel. SliceModel's nbModeChanged feeds
    // back to setNbMode() (wired in the inbound block above).
    // From Thetis console.cs:43513 [v2.10.3.13].
    connect(vfo, &VfoWidget::nbModeCycled, this, [slice] {
        slice->setNbMode(NereusSDR::cycleNbMode(slice->nbMode()));
    });

    // NR/ANF → RxChannel directly (not SliceModel properties)
    connect(vfo, &VfoWidget::nrChanged, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setNrEnabled(on); }
    });
    connect(vfo, &VfoWidget::anfChanged, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setAnfEnabled(on); }
    });

    // --- VfoWidget → SliceModel: DSP tab outbound (S1.8b) ---
    connect(vfo, &VfoWidget::nr2Changed, this, [slice](bool on) {
        // NR2 = EMNR in Thetis naming. Toggle: NR2→active clears any other slot.
        slice->setActiveNr(on ? NereusSDR::NrSlot::NR2 : NereusSDR::NrSlot::Off);
    });
    connect(vfo, &VfoWidget::snbChanged, this, [slice](bool on) {
        slice->setSnbEnabled(on);
    });
    connect(vfo, &VfoWidget::apfChanged, this, [slice](bool on) {
        slice->setApfEnabled(on);
    });
    connect(vfo, &VfoWidget::apfTuneHzChanged, this, [slice](int hz) {
        slice->setApfTuneHz(hz);
    });

    // --- SliceModel → VfoWidget: Audio tab inbound (S1.8c stubs) ---
    connect(slice, &SliceModel::mutedChanged, this, [vfo](bool v) {
        vfo->setMuted(v);
    });
    connect(slice, &SliceModel::audioPanChanged, this, [vfo](double p) {
        vfo->setAudioPan(p);
    });
    connect(slice, &SliceModel::ssqlEnabledChanged, this, [vfo](bool v) {
        vfo->setSsqlEnabled(v);
    });
    connect(slice, &SliceModel::ssqlThreshChanged, this, [vfo](double d) {
        vfo->setSsqlThresh(d);
    });
    connect(slice, &SliceModel::agcThresholdChanged, this, [vfo](int v) {
        vfo->setAgcThreshold(v);
    });
    connect(slice, &SliceModel::binauralEnabledChanged, this, [vfo](bool v) {
        vfo->setBinauralEnabled(v);
    });

    // --- VfoWidget → SliceModel: Audio tab outbound (S1.8c stubs) ---
    connect(vfo, &VfoWidget::muteChanged, this, [slice](bool v) {
        slice->setMuted(v);
    });
    connect(vfo, &VfoWidget::panChanged, this, [slice](double p) {
        slice->setAudioPan(p);
    });
    connect(vfo, &VfoWidget::squelchEnabledChanged, this, [slice](bool v) {
        slice->setSsqlEnabled(v);
    });
    connect(vfo, &VfoWidget::squelchThreshChanged, this, [slice](int v) {
        slice->setSsqlThresh(static_cast<double>(v));
    });
    connect(vfo, &VfoWidget::agcThreshChanged, this, [slice](int v) {
        slice->setAgcThreshold(v);
    });
    connect(vfo, &VfoWidget::binauralChanged, this, [slice](bool v) {
        slice->setBinauralEnabled(v);
    });
    connect(vfo, &VfoWidget::quickModeRequested, this, [slice](int index) {
        // Quick-mode buttons: 0=USB, 1=CW, 2=DIG (matching AetherSDR defaults)
        static constexpr DSPMode kQuickModes[] = {DSPMode::USB, DSPMode::CWU, DSPMode::DIGU};
        if (index >= 0 && index < 3) {
            slice->setDspMode(kQuickModes[index]);
        }
    });

    // --- VfoWidget → MainWindow: open Setup dialog to AGC/ALC page ---
    connect(vfo, &VfoWidget::openSetupRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("AGC/ALC"));
        dialog->show();
    });

    // --- VfoWidget → Setup → DSP → NB/SNB page (right-click on NB or SNB).
    // Mirrors Thetis chkNB_MouseDown / chkDSPNB2_MouseDown (console.cs:44447
    // [v2.10.3.13]) which call ShowSetupTab(NB_Tab).
    connect(vfo, &VfoWidget::openNbSetupRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("NB/SNB"));
        dialog->show();
    });

    // --- VfoWidget → Setup → DSP → NR/ANF page (Task 18, Sub-epic C-1).
    // Emitted from DspParamPopup "More Settings…" on any NR bank button.
    // Mirrors Thetis chkNR_MouseDown (console.cs [v2.10.3.13]) which calls
    // ShowSetupTab(NR_Tab). Sub-tab selection per NrSlot is deferred to Task 17.
    connect(vfo, &VfoWidget::openNrSetupRequested, this,
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

    // --- VfoWidget AUTO button → SliceModel auto-AGC toggle ---
    connect(vfo, &VfoWidget::autoAgcToggled,
            slice, &SliceModel::setAutoAgcEnabled);

    // --- SliceModel auto-AGC state → update visuals on both widgets ---
    connect(slice, &SliceModel::autoAgcEnabledChanged, this, [this, vfo, slice](bool on) {
        auto* nft = m_radioModel->noiseFloorTracker();
        float nf = nft ? nft->noiseFloor() : -200.0f;
        double offset = slice->autoAgcOffset();
        vfo->updateAgcAutoVisuals(on, nf, offset);
        if (m_rxApplet) {
            m_rxApplet->updateAgcAutoVisuals(on, nf, offset);
        }
    });

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
    connect(vfo, &VfoWidget::ritEnabledChanged, this, [slice](bool on) {
        slice->setRitEnabled(on);
    });

    connect(vfo, &VfoWidget::ritHzChanged, this, [slice](int hz) {
        slice->setRitHz(hz);
    });

    connect(vfo, &VfoWidget::xitEnabledChanged, this, [slice](bool on) {
        slice->setXitEnabled(on);
    });

    connect(vfo, &VfoWidget::xitHzChanged, this, [slice](int hz) {
        slice->setXitHz(hz);
    });

    // --- VfoWidget → SliceModel: STEP cycle (S1.8a — wires to live setStepHz) ---
    connect(vfo, &VfoWidget::stepCycleRequested, this, [slice]() {
        int current = slice->stepHz();
        int next = kStageOneStepLadder[0];
        for (int i = 0; i < kStageOneStepLadderSize; ++i) {
            if (kStageOneStepLadder[i] == current) {
                next = kStageOneStepLadder[(i + 1) % kStageOneStepLadderSize];
                break;
            }
        }
        // setStepHz emits stepHzChanged which the :1626-1629 handler uses to
        // propagate to m_spectrumWidget->setStepSize and vfo->setStepHz.
        slice->setStepHz(next);
    });

    // --- VfoWidget → SliceModel: lock state (S1.8a — verifying edge exists) ---
    connect(vfo, &VfoWidget::lockChanged, this, [slice](bool locked) {
        slice->setLocked(locked);
    });

    // --- SliceModel → VfoWidget: lock state inbound (S1.8a review — I3) ---
    // Without this edge, programmatic changes to SliceModel::locked (e.g. from
    // a future CAT/TCI command) would not be reflected in either lock button.
    connect(slice, &SliceModel::lockedChanged, this, [vfo](bool v) {
        vfo->setLocked(v);
    });

    // closeRequested → removeSlice wiring deferred to S1.10 — Stage 1
    // has no sliceRemoved cleanup path yet, so calling removeSlice leaves
    // dangling VfoWidget + lambda captures. See code review findings.

    connect(vfo, &VfoWidget::sliceActivationRequested, this, [this](int index) {
        m_radioModel->setActiveSlice(index);
    });

    // --- Spectrum click-to-tune → slice ---
    connect(m_spectrumWidget, &SpectrumWidget::frequencyClicked,
            this, [slice](double hz) {
        slice->setFrequency(hz);
    });

    // --- Spectrum filter edge drag → slice ---
    connect(m_spectrumWidget, &SpectrumWidget::filterEdgeDragged,
            this, [slice](int low, int high) {
        slice->setFilter(low, high);
    });

    // --- Pan center changed (pan drag) ---
    // CTUN mode (SmartSDR): retune DDC to pan center, offset WDSP to keep
    // demodulating at VFO frequency. This lets the spectrum show real data
    // across the full pan range.
    // Traditional mode: pan drag retunes the VFO (DDC follows VFO naturally).
    connect(m_spectrumWidget, &SpectrumWidget::centerChanged,
            this, [this, slice](double centerHz) {
        if (m_handlingBandJump) {
            return;
        }
        if (!m_spectrumWidget->ctunEnabled()) {
            slice->setFrequency(centerHz);
        } else {
            // CTUN: retune DDC to pan center (bypasses lock) so spectrum shows correct data
            int rxIdx = slice->receiverIndex();
            if (rxIdx >= 0) {
                m_radioModel->receiverManager()->forceHardwareFrequency(
                    rxIdx, static_cast<quint64>(centerHz));
            }
            m_spectrumWidget->setDdcCenterFrequency(centerHz);
            // Offset WDSP shift so audio stays on VFO frequency
            // From Thetis radio.cs:1417 — SetRXAShiftFreq receives +(freq - center)
            double shiftHz = slice->frequency() - centerHz;
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(shiftHz);
            }
        }
    });

    // --- CTUN mode toggled → lock/unlock DDC ---
    connect(m_spectrumWidget, &SpectrumWidget::ctunEnabledChanged,
            this, [this](bool enabled) {
        m_radioModel->receiverManager()->setDdcFrequencyLocked(enabled);
        if (!enabled) {
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(0.0);
            }
        }
    });

    // --- dBm range strip → PanadapterModel (per-band grid storage + AppSettings) ---
    connect(m_spectrumWidget, &SpectrumWidget::dbmRangeChangeRequested,
            this, [this](float minDbm, float maxDbm) {
        if (m_radioModel && !m_radioModel->panadapters().isEmpty()) {
            PanadapterModel* pan = m_radioModel->panadapters().first();
            pan->setdBmFloor(static_cast<int>(minDbm));
            pan->setdBmCeiling(static_cast<int>(maxDbm));
        }
    });

    // Set initial lock state
    m_radioModel->receiverManager()->setDdcFrequencyLocked(
        m_spectrumWidget->ctunEnabled());

    // Position the VFO flag
    m_spectrumWidget->updateVfoPositions();

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
    if (m_overlayPanel) {
        connect(m_overlayPanel, &SpectrumOverlayPanel::bandSelected,
                this, [this](const QString& name, double /*freqHz*/, const QString& /*mode*/) {
            m_radioModel->onBandButtonClicked(bandFromName(name));
        });
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
        // 3. PA voltage (MKII-class only; widget is hidden on non-MKII boards
        //    — drop logic skips invisible widgets so this is a no-op there).
        {{ m_paVoltLabel, m_paVoltLabelSep, tr("PA voltage") }},
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
    // Phase 3Q-8: forward state to the spectrum widget for the disconnect overlay.
    if (m_spectrumWidget) {
        m_spectrumWidget->setConnectionState(m_radioModel->connectionState());
    }

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
            // Phase 3Q Task 10: surface the failure — open panel + status bar.
            const QString name = AppSettings::instance()
                .savedRadio(chosenMac)
                .value_or(SavedRadio{})
                .info.name;
            const QString displayName = name.isEmpty() ? chosenMac : name;
            showConnectionPanel();
            if (m_connectionPanel) {
                m_connectionPanel->highlightMac(chosenMac);
            }
            statusBar()->showMessage(
                QStringLiteral("Auto-connect target %1 isn't reachable from this network. "
                               "Pick a different radio or update the address.")
                    .arg(displayName),
                8000);
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
    if (m_spectrumWidget) {
        m_spectrumWidget->saveSettings();
    }

    // Tear down connection (sends stop command, closes sockets, joins thread)
    m_radioModel->disconnectFromRadio();

    // Save container layout
    if (m_containerManager) {
        m_containerManager->saveState();
    }

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
