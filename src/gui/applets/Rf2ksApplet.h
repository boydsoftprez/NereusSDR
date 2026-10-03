// =================================================================
// src/gui/applets/Rf2ksApplet.h  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   Layout patterns from src/gui/applets/AmpApplet.{h,cpp} (which is
//   an AetherSDR port). The RF-Kit-specific content is original.
//   2026-09-23  R-R3-47 / R-R3-22: the header, gauges and strip read the
//   RadioModel's RfKitModel (the Core's `rfkit` object in a remote
//   window), with a stale line when a remote window loses the Core.
//   J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24  R-R3-22: a remote window's Disconnect and Reconnect ask
//   the Core, with a line for its connection and any refusal. J.J. Boyd
//   (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25  R-R3-49 (parity Task 10): a remote window's OPERATE and ANT
//   1 to 4 ask the Core (setRfKitOperate, setRfKitAntenna,
//   remoteRfKitControlVersion 4), wait while the radio is on the air, and
//   follow the amp's report; coreDiagnosticsText. J.J. Boyd (KG4VCF),
//   AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once
#include "AppletWidget.h"
#include "core/Rf2ksConnection.h"
#include "models/TunerModel.h"

#include <QHash>
#include <QSet>
#include <QPushButton>

class QContextMenuEvent;
class QLabel;
class QMenu;

namespace NereusSDR {

class HGauge;
class RadioModel;
class RfKitModel;

// Rf2ksApplet -- RF-Kit RF2K-S power amplifier control applet.
//
// Section A (Task 7): header row only.
//   - Device label ("RF-Kit RF2K-S"), nickname/version label below it.
//   - Status dot (green=connected, red=disconnected).
//   - OPERATE/STANDBY toggle button: emits operateToggled(bool).
//
// Section B (Task 8): gauges + telemetry strip.
//   - 3 HGauge bars (Fwd, SWR, Temp).
//   - Telemetry label: mains voltage + drain current.
//
// Section C (Task 8): antennas + tuner status + greyed TUNE/BYPASS.
//   - 4 antenna buttons with operator-set labels or "ANT N" fallback.
//   - Tuner status line ("TUNED X.XXX MHz (LC)" / "TUNING..." / "BYPASS").
//   - TUNE + BYPASS buttons disabled (firmware limitation).
//
// Section D (Task 9): right-click context menu.
//
// Layout patterns from AmpApplet.{h,cpp} (AetherSDR port, GPLv3).
class Rf2ksApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit Rf2ksApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("RfKit"); }
    QString appletTitle() const override { return QStringLiteral("RF-Kit RF2K-S"); }
    // R-R3-47: fills the header, gauges and strip from the RfKitModel.
    void    syncFromModel() override { syncFromRfKit(); }

    // Test seams (Section A - Task 7).
    QString deviceLabelTextForTesting()    const;
    QString nicknameLabelTextForTesting()  const;
    QString operateButtonTextForTesting()  const;
    void    clickOperateButtonForTesting();

    // Test seam (Section D - Task 9).
    QMenu*  buildContextMenuForTesting() { return buildContextMenu(this); }

    // Test seams (Sections B+C - Task 8).
    int     fwdGaugeValueForTesting()                  const;
    float   swrGaugeValueForTesting()                  const;
    float   tempGaugeValueForTesting()                 const;
    QString telemetryStripTextForTesting()             const;
    QString antennaButtonTextForTesting(int number)    const;
    bool    antennaButtonIsActiveForTesting(int number)   const;
    bool    antennaButtonIsEnabledForTesting(int number)  const;
    void    clickAntennaButtonForTesting(int number);
    QString tunerStatusTextForTesting()                const;
    bool    tuneButtonIsEnabledForTesting()            const;
    bool    bypassButtonIsEnabledForTesting()          const;
    QString tuneButtonTooltipForTesting()              const;
    // Test seams (R-R3-47).
    bool    connectedStateForTesting()                 const { return m_connected; }
    bool    staleIndicatorVisibleForTesting()          const;
    QString staleIndicatorTextForTesting()             const;
    QString bandFollowTextForTesting()                 const;
    // R-R3-22: a remote window's connection line ("" when hidden).
    QString connectionLineTextForTesting()             const;
    // R-R3-49 (parity Task 10).
    bool    operateButtonEnabledForTesting()           const { return m_operateBtn->isEnabled(); }
    QString operateButtonToolTipForTesting()           const { return m_operateBtn->toolTip(); }
    QString antennaButtonToolTipForTesting(int number) const;

    // R-R3-49 (parity Task 10): the Core's RF-Kit diagnostics for a remote
    // window's Copy diagnostics: the mirrored `rfkit` object and
    // accessoryData's rfkit* counters, never this computer's idle
    // connection.
    static QString coreDiagnosticsText(RadioModel* model);

signals:
    // Emitted when the user clicks the OPERATE/STANDBY toggle button.
    // requestedOperate=true means the user wants to enter OPERATE state.
    void operateToggled(bool requestedOperate);

    // Emitted when the user clicks one of the antenna buttons.
    void antennaRequested(RfKitAntenna::Type type, int number);

    // Right-click context menu signals (filled in Task 9).
    void navigationRequested(const QString& pageKey);
    // Local windows only: a remote window asks the Core itself (R-R3-22).
    void connectionToggleRequested();
    void diagnosticsCopyRequested();

public slots:
    // Section A slots (Task 7).
    void setNicknameAndVersion(const QString& nickname, const QString& version);
    void setOperateMode(const QString& mode);   // "OPERATE" or "STANDBY"
    void setConnectedState(bool connected);

    // Section B slots (Task 8).
    void setPower(const RfKitPowerSnapshot& snap);

    // Section C slots (Task 8).
    void setTuner(const RfKitTunerSnapshot& snap);
    void setAntennas(const QList<RfKitAntenna>& list);
    void setActiveAntenna(const RfKitAntenna& a);
    void setAntennaLabel(int number, const QString& label);

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;

private slots:
    // R-R3-47: the RfKitModel's readings into the header, gauges and strip.
    void syncFromRfKit();
    // R-R3-47: a remote window says when its readings are not live.
    void updateStationState();
    // R-R3-22: a remote window's connection line: the Core's phase, or the
    // plain reason its last Disconnect or Connect was not taken.
    void updateConnectionLine();
    // R-R3-22 fix wave: the Core's answer to a command, by id; only the
    // applet's own Disconnect or Connect (m_pendingCommandId) is shown.
    void onStationCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    // R-R3-49 (parity Task 10): a remote window's OPERATE and antenna
    // buttons: enabled on a Core at remoteRfKitControlVersion 4 that is
    // connected to the amp while the radio is off the air (an antenna also
    // only if the amp lists it as usable); otherwise disabled with the
    // reason.
    void updateRemoteControls();

private:
    QMenu* buildContextMenu(QObject* menuParent);
    bool   isRemoteModel() const;
    // Group B fix wave (M5): a local window's own switch, refused on the
    // air; parity mini-round (ruling c): the refusal is shown with the
    // remote window's reason (RadioModel::refuseLocalAccessorySwitchOnAir).
    bool   refuseLocalSwitchOnAir();
    // R-R3-22: a remote window's Disconnect or Connect, sent to the Core.
    void   requestRemoteConnectionToggle();
    // R-R3-49 (parity Task 10): the Core switches its amp for this window.
    bool   remoteFullControl() const;
    // Why a remote window's OPERATE and antennas wait ("" when they do not).
    QString remoteControlReason() const;

    // Section A widgets.
    QLabel*      m_deviceLabel{nullptr};
    QLabel*      m_nicknameLabel{nullptr};
    QPushButton* m_operateBtn{nullptr};
    QLabel*      m_statusDot{nullptr};
    QString      m_operateMode;
    bool         m_connected{false};

    // Section B widgets.
    HGauge* m_fwdGauge{nullptr};
    HGauge* m_swrGauge{nullptr};
    HGauge* m_tempGauge{nullptr};
    QLabel* m_telemetryLabel{nullptr};

    // Section C widgets.
    QHash<int, QPushButton*> m_antennaButtons;
    // Group B fix wave (M5): the antennas this computer's amp lists as
    // disabled (a local window).
    QSet<int> m_localAntennaDisabled;
    QHash<int, QString>      m_antennaLabels;
    QLabel*                  m_tunerStatusLabel{nullptr};
    QPushButton*             m_tuneBtn{nullptr};
    QPushButton*             m_bypassBtn{nullptr};

    // R-R3-47: the readings this applet shows, and a remote window's line
    // saying they are stale (Core lost) or not offered (older Core).
    RfKitModel* m_rfKit{nullptr};
    QLabel*     m_staleLabel{nullptr};
    // R-R3-48: the band-follow line.
    QLabel*     m_bandFollowLabel{nullptr};
    // R-R3-22: a remote window's connection line, the id of its own request
    // while it waits on the Core, and the reason one was not taken.
    QLabel*     m_connectionLabel{nullptr};
    quint32     m_pendingCommandId{0};  // 0: nothing of the applet's own waiting
    QString     m_requestReason;
    TunerModel::ConnectionPhase m_lastPhase{TunerModel::ConnectionPhase::Disabled};
};

} // namespace NereusSDR
