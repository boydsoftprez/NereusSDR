// =================================================================
// src/gui/applets/AmpApplet.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/AmpApplet.{h,cpp} [@0cd4559].
//                 Changes from upstream: inherit AppletWidget (not QWidget);
//                 constructor takes RadioModel*; HGauge uses NereusSDR setter
//                 API (setRange/setYellowStart/setRedStart/setTitle/setUnit)
//                 instead of upstream positional constructor; public slots
//                 added (upstream used public methods); appletId/appletTitle/
//                 syncFromModel pure-virtual overrides added.
//   2026-09-23  R-R3-47 / R-R3-22: the gauges read the RadioModel's
//                 AmplifierModel (the Core's `amplifier` object in a remote
//                 window, the same object in-process locally), with a stale
//                 line when a remote window loses the Core. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24  R-R3-22 / R-R3-47: a remote window's Disconnect and
//                 Reconnect ask the Core (disconnectPgxl, configurePgxl)
//                 and a status line shows the Core's connection and any
//                 refusal. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25  R-R3-49 (parity Task 9): a remote window's OPERATE asks
//                 the Core (setPgxlOperate, remotePgxlControlVersion 4),
//                 waits while the radio is on the air, and follows the
//                 amp's reported state; coreDiagnosticsText. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26  iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03,
//                 R-IOS-13): OPERATE waits (disabled, with the reason) while
//                 a Tuner Genius cycle runs, in both windows; a faulted
//                 amp's click puts it in standby. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once
#include "AppletWidget.h"
#include "models/TunerModel.h"

#include <QPushButton>

class QContextMenuEvent;
class QLabel;
class QMenu;

namespace NereusSDR {

class AmplifierModel;
class HGauge;
class RadioModel;

// AmpApplet -- PGXL power amplifier telemetry and control applet.
//
// Displays three HGauge bars (Fwd Power, SWR, Temp) plus a stacked
// telemetry row showing mains voltage, drain current (Amps), and
// mains efficiency (MEffA). An OPERATE/STANDBY toggle button reflects
// the current amplifier state and emits operateToggled() on click.
//
// Right-click context menu (Phase 3P-II Phase 4 Task 88):
//   Open PGXL Advanced...        -> navigationRequested("pgxlAdvanced")
//   (separator)
//   Disconnect / Connect        -> connectionToggleRequested()
//   Copy diagnostics to clipboard -> diagnosticsCopyRequested()
//
// From AetherSDR src/gui/AmpApplet.h [@0cd4559]
class AmpApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit AmpApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("amp"); }
    QString appletTitle() const override { return QStringLiteral("Power Genius"); }
    // R-R3-47: fills the gauges from the RadioModel's AmplifierModel.
    void    syncFromModel() override { syncFromAmplifier(); }

    // Test seam: returns a heap-allocated QMenu* without exec()-ing it.
    // Caller owns the returned menu; delete or deleteLater() as needed.
    // Same pattern as SMeterWidget::buildContextMenuForTesting() (Task 38,
    // commit 067d2d5b).
    QMenu* buildContextMenuForTesting() { return buildContextMenu(this); }

    // R-R3-21: why OPERATE and the connection toggle are unavailable in a
    // remote session. Public so the RF-Kit applet, which drives the same
    // kind of station-side amplifier, gives the same reason.
    static QString remoteUnavailableReason();

    // R-R3-22 / R-R3-47: a station accessory's connection as a remote
    // window shows it, in user words. Shared with the RF-Kit applet.
    static QString stationConnectionText(TunerModel::ConnectionPhase phase,
                                         const QString& error);
    // R-R3-22: the phases in which the Core is connected to, or trying to
    // reach, the accessory; Disconnect then cancels or closes it.
    static bool stationConnectionActive(TunerModel::ConnectionPhase phase);
    // R-R3-22 fix wave: the remote toggle's words for a phase, as the
    // Peripherals row says them: Disconnect when connected, Cancel while
    // the Core is still trying, Connect otherwise.
    static QString stationConnectionToggleText(TunerModel::ConnectionPhase phase);
    // R-R3-49 (parity Task 9): the Core's Power Genius diagnostics for a
    // remote window's Copy diagnostics: the mirrored `amplifier` object and
    // accessoryData's pgxl* counters, never this computer's idle socket.
    static QString coreDiagnosticsText(RadioModel* model);
    bool operateButtonEnabledForTesting() const { return m_operateBtn->isEnabled(); }
    QString operateButtonToolTipForTesting() const { return m_operateBtn->toolTip(); }
    void clickOperateForTesting() { m_operateBtn->click(); }

    // Test seams (R-R3-47).
    double  fwdGaugeValueForTesting() const;
    double  swrGaugeValueForTesting() const;
    double  tempGaugeValueForTesting() const;
    QString powerLabelTextForTesting() const;
    QString meffLabelTextForTesting() const;
    QString operateButtonTextForTesting() const;
    bool    operateButtonShownForTesting() const;
    bool    staleIndicatorVisibleForTesting() const;
    QString staleIndicatorTextForTesting() const;
    QString bandFollowTextForTesting() const;
    // R-R3-22: a remote window's connection line ("" when hidden).
    QString connectionLineTextForTesting() const;

signals:
    // Emitted when the user clicks the OPERATE/STANDBY button.
    // requestedOperate=true means the user wants to enter OPERATE state.
    // From AetherSDR src/gui/AmpApplet.h:26 [@0cd4559]
    void operateToggled(bool requestedOperate);

    // Phase 3P-II Phase 4 Task 88: right-click context menu signals.

    // Emitted when "Open PGXL Advanced..." is triggered.
    // pageKey is "pgxlAdvanced"; MainWindow::openSetup() is the handler.
    void navigationRequested(const QString& pageKey);

    // Emitted when "Disconnect" / "Connect" is triggered in a local
    // window. MainWindow should call pgxlConnection()->disconnectFromPgxl()
    // or reconnect depending on current state. A remote window asks the
    // Core itself and does not emit this (R-R3-22).
    void connectionToggleRequested();

    // Emitted when "Copy diagnostics to clipboard" is triggered.
    // MainWindow assembles the diagnostic string from the connection and
    // copies it to QClipboard.
    void diagnosticsCopyRequested();

public slots:
    // From AetherSDR src/gui/AmpApplet.h:17-23 [@0cd4559]
    void setFwdPower(float w);
    void setSwr(float v);
    void setTemp(float c);
    void setDrainCurrent(float a);
    void setMainsVoltage(int v);
    void setState(const QString& state);
    void setMeff(const QString& meff);

    // 2026-05-22: track last-seen state so MainWindow's PGXL status
    // handler can gate peakfwd/swr writes on it. PGXL keeps reporting
    // a latched peak value after the amp leaves TRANSMIT_A/B; without
    // this query the AmpApplet gauges latched at the last TX peak.
    bool isTransmitting() const { return m_isTransmitting; }

    // Phase 3P-II Phase 4 Task 88: update the connected flag so the
    // context menu shows "Disconnect" vs "Connect" appropriately.
    void setPgxlConnected(bool connected);

protected:
    // Phase 3P-II Phase 4 Task 88: right-click context menu.
    void contextMenuEvent(QContextMenuEvent* ev) override;

private slots:
    // R-R3-47: the AmplifierModel's readings into the gauges and labels.
    void syncFromAmplifier();
    // R-R3-47: a remote window says when its readings are not live.
    void updateStationState();
    // R-R3-22: a remote window's connection line: the Core's phase, or the
    // plain reason its last Disconnect or Connect was not taken.
    void updateConnectionLine();
    // R-R3-22 fix wave: the Core's answer to a command, by id; only the
    // applet's own Disconnect or Connect (m_pendingCommandId) is shown.
    void onStationCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    // R-R3-49 (parity Task 9): a remote window's OPERATE: enabled on a Core
    // at remotePgxlControlVersion 4 that is connected to the amp while the
    // radio is off the air; otherwise disabled with the reason.
    void updateOperateButton();

private:
    // R-R3-22: a remote window's Disconnect or Connect, sent to the Core.
    void requestRemoteConnectionToggle();
    bool isRemoteWindow() const;
    // iPhone app plan Task 77 fix round 3: the amp reports a fault (its
    // OPERATE button then sends standby), and the tooltip saying so.
    bool ampFaulted() const;
    QString faultedTip() const;
    // R-R3-49 (parity Task 9): the Core puts its amp in operate or standby
    // for this remote window.
    bool remoteOperateControl() const;

    // From AetherSDR src/gui/AmpApplet.h:29 [@0cd4559]
    void updatePowerLabel();

    // Phase 3P-II Phase 4 Task 88: builds the context menu.
    // parent is the QObject* parent for the returned heap-allocated QMenu.
    QMenu* buildContextMenu(QObject* menuParent);

    // From AetherSDR src/gui/AmpApplet.h:31-38 [@0cd4559]
    HGauge*      m_fwdGauge{nullptr};
    HGauge*      m_swrGauge{nullptr};
    HGauge*      m_tempGauge{nullptr};
    QLabel*      m_powerLabel{nullptr};
    QLabel*      m_meffLabel{nullptr};
    QPushButton* m_operateBtn{nullptr};
    int          m_mainsVolts{0};
    float        m_drainAmps{0};

    // Phase 3P-II Phase 4 Task 88: PGXL connection state for context menu label.
    bool m_pgxlConnected{false};

    // 2026-05-22 bench fix: cached transmitting flag set by setState
    // (true when state == TRANSMIT_A/B). Read by isTransmitting() so
    // MainWindow's PGXL status handler can gate peakfwd/swr forwarding.
    bool m_isTransmitting{false};

    // R-R3-47: the readings this applet shows, and a remote window's line
    // saying they are stale (Core lost) or not offered (older Core).
    AmplifierModel* m_amp{nullptr};
    QLabel*         m_staleLabel{nullptr};
    // R-R3-48: the band-follow line.
    QLabel*         m_bandFollowLabel{nullptr};
    // R-R3-22: a remote window's connection line, the id of its own request
    // while it waits on the Core, and the reason one was not taken.
    QLabel*         m_connectionLabel{nullptr};
    quint32         m_pendingCommandId{0};  // 0: nothing of the applet's own waiting
    QString         m_requestReason;
    TunerModel::ConnectionPhase m_lastPhase{TunerModel::ConnectionPhase::Disabled};
};

} // namespace NereusSDR
