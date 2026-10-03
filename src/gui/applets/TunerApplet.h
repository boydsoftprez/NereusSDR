// =================================================================
// src/gui/applets/TunerApplet.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 ss.5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18  Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/TunerApplet.{h,cpp}
//                 (ATU/tune controls + SWR progress bar). All controls
//                 NYI -- wired in later phase.
//   2026-05-18  Rewired for TGXL by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 NyiOverlay removed; QProgressBar relay bars replaced
//                 with RelayBar; 3-button mode group replaced with single
//                 cycle button; constructor wired to TunerModel*.
//                 From AetherSDR src/gui/TunerApplet.h [@0cd4559].
//   2026-09-24  R-R3-49 / R-R3-47 by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 Remote ANT/OPERATE through the Core (see .cpp).
//   2026-09-25  R-R3-49 (parity Task 8) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 Remote relay nudges, tune memory recall, Open TGXL
//                 Advanced and the Core's diagnostics (see .cpp).
//   2026-09-25  transmitBlocked(): TUNE follows the transmit block (Task
//                 16 fix wave M2), by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30: Fix wave GUI-I4: remoteWindow, remoteTuneControl and the
//               three remote reasons. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#pragma once
#include "AppletWidget.h"
#include "models/Band.h"
#include "core/TuneMemoryStore.h"

class QContextMenuEvent;
class QLabel;
class QPushButton;
class QMenu;
class QWidget;
class QTimer;

namespace NereusSDR {

class HGauge;
class RelayBar;
class TunerModel;

// 4O3A Tuner Genius XL (TGXL) controls.
//
// Layout (top to bottom):
//   1. Fwd Power gauge    -- HGauge (0-200W, red@125; rescaled if PGXL present)
//   2. SWR gauge          -- HGauge (1.0-3.0, red@2.5)
//   3. Relay position bars  -- RelayBar x3 ("C1", "L", "C2"), 0-255
//   4. TUNE button        -- QPushButton (non-toggle, sends autoTune; shows
//                            "TUNING..." in red while active, then "SWR X.XX"
//                            for 2.5 s post-tune)
//   5. OPERATE cycle btn  -- Single QPushButton; click cycles OPERATE->BYPASS->
//                            STANDBY->OPERATE; label + color follows state
//   6. Antenna container  -- 3x QPushButton (ANT 1/2/3); hidden unless
//                            hasDirectConnection() && hasAntennaSwitch()
//
// Right-click context menu (Phase 3P-II Phase 4 Task 89):
//   Open TGXL Advanced...                     -> navigationRequested("tgxlAdvanced")
//   (separator)
//   Save current tune memory                  -> store(currentMem())
//   Recall tune memory for current (ant,band) -> apply stored relay positions
//   Clear tune memory for current (ant,band)  -> m_tuneStore->clear(ant,band)
//   (separator)
//   Disconnect / Connect                    -> connectionToggleRequested()
//   Copy diagnostics to clipboard             -> diagnosticsCopyRequested()
//
// From AetherSDR src/gui/TunerApplet.h [@0cd4559]
class TunerApplet : public AppletWidget {
    Q_OBJECT
public:
    // Phase 3P-II Phase 4 Task 89: tuneStore is a non-owning pointer to the
    // RadioModel-owned TuneMemoryStore (shared with TgxlAdvancedPage).
    // Pass nullptr when TuneMemoryStore is not available (e.g. unit tests).
    explicit TunerApplet(RadioModel* model, TunerModel* tunerModel = nullptr,
                         QWidget* parent = nullptr,
                         TuneMemoryStore* tuneStore = nullptr);

    QString appletId()    const override { return QStringLiteral("tuner"); }
    QString appletTitle() const override { return QStringLiteral("Tuner Genius"); }
    void    syncFromModel() override;

    // Attach (or replace) the TunerModel. Safe to call with nullptr.
    void setTunerModel(TunerModel* model);

    // Switch Fwd Power gauge scale: barefoot (0-200W, red@125) vs PGXL (0-2000W, red@1500).
    // From AetherSDR src/gui/TunerApplet.h:setPowerScale [@0cd4559]
    void setPowerScale(int maxWatts, bool hasAmplifier);

    // Test seam: returns a heap-allocated QMenu* without exec()-ing it.
    // Caller owns the returned menu; delete or deleteLater() as needed.
    // Same pattern as AmpApplet/SMeterWidget buildContextMenuForTesting().
    QMenu* buildContextMenuForTesting() { return buildContextMenu(this); }

    // Phase 3P-II Phase 4 Task 89: test seams for context menu tests.
    // These are public so tst_tuner_applet_context_menu can call them
    // without needing RadioModel / TunerModel live instances.
    void testSetCurrentBandAndAntenna(Band band, int antenna)
    {
        m_currentBand    = band;
        m_currentAntenna = antenna;
    }
    void testSetRelayValues(int c1, int l, int c2)
    {
        m_lastC1 = c1;
        m_lastL  = l;
        m_lastC2 = c2;
    }
    QString tuneButtonTextForTesting() const;
    bool carrierEngagedForTgxlTuneForTesting() const
    {
        return m_carrierEngagedForTgxlTune;
    }
    bool actuatingControlsEnabledForTesting() const;
    // R-R3-49 / R-R3-47: the antenna and OPERATE buttons, for tests.
    QPushButton* antennaButtonForTesting(int port) const;
    QPushButton* operateButtonForTesting() const { return m_operateBtn; }
    QPushButton* tuneButtonForTesting() const { return m_tuneBtn; }
    // R-R3-49 (parity Task 8): the relay bars (0 C1, 1 L, 2 C2), for tests.
    RelayBar* relayBarForTesting(int relay) const;

    // R-R3-49 (parity Task 8): Copy diagnostics to clipboard in a remote
    // window. The Core's connection to its Tuner Genius, from the mirrored
    // `tuner` object and `accessoryData`'s tgxl counters, never this
    // computer's idle connection.
    static QString coreDiagnosticsText(RadioModel* model);

    // R-R3-49 / R-R3-47: the Core's refusal while it transmits, and the
    // tooltip on ANT and OPERATE in a remote window while the radio is on
    // the air.
    // Group B fix wave (M5): RadioModel's one sentence, shared by both
    // windows.
    static QString onAirReason();
    // GUI-I4 (fix wave): the reasons a remote window that may transmit
    // shows when its Core does not run that tuner control for this app.
    static QString noRemoteTuneReason();
    static QString noRemoteTunerReason();
    static QString noRemoteRelayReason();
    bool staleIndicatorVisibleForTesting() const;

    // R3 remote sessions are receive-only. MainWindow applies the negotiated
    // station capability here; telemetry remains visible while every TGXL
    // command surface stays disabled and its handler refuses programmatic
    // activation too.
    void setTransmitPermitted(bool permitted, const QString& reason = QString());

    // The remote model retains last-known telemetry across link loss. This
    // controls an explicit stale-state presentation without erasing it.
    void setStationConnected(bool connected);

    // Phase 3P-II Phase 4 Task 89: update TGXL connected flag for context menu.
    void setTgxlConnected(bool connected);

signals:
    // Phase 3P-II Phase 4 Task 89: right-click context menu signals.

    // Emitted when "Open TGXL Advanced..." is triggered.
    // pageKey is "tgxlAdvanced"; MainWindow::openSetup() is the handler.
    void navigationRequested(const QString& pageKey);

    // Emitted when "Disconnect" / "Connect" is triggered.
    void connectionToggleRequested();

    // Emitted when "Copy diagnostics to clipboard" is triggered.
    void diagnosticsCopyRequested();

public slots:
    // Feed forward power (W) and SWR into the gauges.
    // From AetherSDR src/gui/TunerApplet.h:updateMeters [@0cd4559]
    void updateMeters(float fwdPower, float swr);

    // Phase 3P-II Phase 4 Task 95: live-update a single antenna button label.
    // index is 1..3 (matches AppSettings key suffix TGXL_Ant1_Label etc.).
    // Called from SetupDialog::tgxlAntennaLabelChanged (wired in wireSetupDialog).
    void onAntennaLabelChanged(int index, const QString& label);

    // Phase 3P-II review fix C1: update m_currentBand so Save/Recall/Clear
    // context-menu actions operate on the correct (antenna, band) slot.
    // Wired by MainWindow::wireSliceToSpectrum to SliceModel::bandChanged.
    void setBand(Band band);

protected:
    // Phase 3P-II Phase 4 Task 89: right-click context menu.
    void contextMenuEvent(QContextMenuEvent* ev) override;

private slots:
    // Cycle OPERATE -> BYPASS -> STANDBY -> OPERATE.
    // From AetherSDR src/gui/TunerApplet.cpp:cycleOperateState [@0cd4559]
    void cycleOperateState();

    // Highlight the active antenna button (antA is 0-indexed).
    // From AetherSDR src/gui/TunerApplet.cpp:updateAntennaButtons [@0cd4559]
    void updateAntennaButtons(int antA);

private:
    void buildUI();

    // Phase 3P-II Phase 4 Task 95: reads TGXL_Ant{1,2,3}_Label from AppSettings
    // and applies text to the three antenna QPushButtons.
    // Defaults to "ANT 1" / "ANT 2" / "ANT 3" when a key is empty.
    // Called once at construction (after buildUI) and on-demand via
    // onAntennaLabelChanged.
    void refreshAntennaLabels();

    // Phase 3P-II Phase 4 Task 89: builds the context menu.
    QMenu* buildContextMenu(QObject* menuParent);

    // Build a TuneMemory from the applet's current state.
    TuneMemory currentMem() const;
    void updateActuatingControls();
    // Task 16 fix wave (M2): receive only, TX inhibit or a PA trip holds
    // (MoxController::transmitBlockReason).
    bool transmitBlocked() const;
    void updateStationAvailability();
    // R-R3-49 / R-R3-47: a remote window on a Core that switches its
    // Tuner Genius for this app (remoteTgxlControlVersion 2). ANT and
    // OPERATE then ask the Core and follow its transmit state; TUNE and the
    // relay bars keep the transmit permission.
    bool remoteTunerControl() const;
    // R-R3-49 (parity Task 8): the same Core also moves the relays for this
    // app (remoteTgxlControlVersion 4). The relay bars then ask the Core
    // and wait while the radio is on the air.
    bool remoteRelayControl() const;
    void requestRelayMove(int relay, int direction);
    // GUI-I4 (fix wave): a remote window. Its tuner controls act only
    // through the Core; on a Core that does not offer one, that control
    // is disabled with the reason and never falls back to this computer's
    // own Tuner Genius.
    bool remoteWindow() const;
    // A remote window on a Core that runs a Tuner Genius tune for this app
    // (IStationLink::tgxlAutotuneAvailable).
    bool remoteTuneControl() const;
    // The Core reports the radio on the air: MOX, TUNE or two-tone.
    bool coreOnAir() const;
    // Group B fix wave (M5): a local window's own switch, refused on the
    // air; parity mini-round (ruling c): the refusal is shown with the
    // remote window's reason (RadioModel::refuseLocalAccessorySwitchOnAir).
    bool refuseLocalSwitchOnAir();
    void requestAntenna(int port);

    TunerModel* m_tunerModel = nullptr;

    // Control 1 -- Forward power gauge (0-200W, red@125; auto-rescaled)
    HGauge* m_fwdPowerGauge = nullptr;
    // Control 2 -- SWR gauge (1.0-3.0, red@2.5)
    HGauge* m_swrGauge       = nullptr;
    QLabel* m_staleLabel     = nullptr;

    // Control 3 -- Relay position bars (C1 / L / C2)
    // From AetherSDR src/gui/TunerApplet.h:m_c1Bar [@0cd4559]
    RelayBar* m_c1Bar = nullptr;
    RelayBar* m_lBar  = nullptr;
    RelayBar* m_c2Bar = nullptr;

    // Control 4 -- TUNE button (non-toggle)
    QPushButton* m_tuneBtn = nullptr;

    // Control 5 -- Single cycle button (OPERATE / BYPASS / STANDBY)
    // From AetherSDR src/gui/TunerApplet.h:m_operateBtn [@0cd4559]
    QPushButton* m_operateBtn = nullptr;

    // Control 6 -- Antenna container (3 buttons); shown only when direct
    // connection active and antenna switch present.
    // From AetherSDR src/gui/TunerApplet.h:m_antContainer [@0cd4559]
    QWidget*     m_antContainer = nullptr;
    QPushButton* m_ant1Btn      = nullptr;
    QPushButton* m_ant2Btn      = nullptr;
    QPushButton* m_ant3Btn      = nullptr;

    // Meter values (updated by updateMeters)
    // From AetherSDR src/gui/TunerApplet.h:m_fwdPower [@0cd4559]
    float m_fwdPower{0.0f};
    float m_swr{1.0f};

    // Post-tune SWR capture state
    // From AetherSDR src/gui/TunerApplet.h:m_wasTuning [@0cd4559]
    bool   m_wasTuning{false};
    bool   m_postTuneCapture{false};
    float  m_tuneSwr{1.0f};
    QTimer* m_postTuneTimer{nullptr};

    // Latch: did *we* engage the local tune-carrier via MoxController::setTune
    // for the current TGXL tune cycle? If so, drop it on tuning=0. We do NOT
    // touch MOX if the operator already has TUN engaged via the TxApplet
    // TUNE button (isManualMox() was true at the moment we evaluated).
    // Without this latch a TGXL tune cycle would release an operator-
    // initiated TUN when it finished, which would surprise the operator.
    // NereusSDR-native: no AetherSDR equivalent because AetherSDR talks to a
    // real FlexRadio that handles the carrier internally.
    bool   m_carrierEngagedForTgxlTune{false};

    // Has TGXL entered its own tuning=1 state since the last TUNE click?
    // Reset on each TUNE click; set true on tuningChanged(true). Used by
    // the short-watchdog escape path: if 3 s elapses after `autotune` was
    // sent and m_tgxlEnteredTuning is still false, TGXL aborted (likely
    // "no PTT" / amp-in-OPERATE confusion) and the watchdog drops the
    // stuck local carrier. Long tune cycles (TGXL relay sweep up to ~30 s)
    // are gated by m_tgxlEnteredTuning being true -- the carrier rides
    // until tuningChanged(false) arrives normally.
    bool   m_tgxlEnteredTuning{false};

    // Phase 3P-II Phase 4 Task 89: context menu state.
    // Non-owning pointer to the RadioModel-owned TuneMemoryStore.
    TuneMemoryStore* m_tuneStore{nullptr};
    // Current band and antenna (updated by TunerModel signals or test seams).
    Band m_currentBand{Band::Band20m};
    int  m_currentAntenna{1};        // 1-indexed (1..3)
    // Last relay values received from TunerModel (used by Save).
    int  m_lastC1{0};
    int  m_lastL{0};
    int  m_lastC2{0};
    // TGXL connected state for Disconnect/Connect label.
    bool m_tgxlConnected{false};
    bool m_transmitPermitted{true};
    QString m_transmitPermissionReason;
    bool m_stationConnected{true};
};

} // namespace NereusSDR
