// =================================================================
// src/gui/applets/Rf2ksApplet.h  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   Layout patterns from src/gui/applets/AmpApplet.{h,cpp} (which is
//   an AetherSDR port). The RF-Kit-specific content is original.
// =================================================================
#pragma once
#include "AppletWidget.h"
#include "core/Rf2ksConnection.h"

#include <QHash>
#include <QPushButton>

class QContextMenuEvent;
class QLabel;
class QMenu;

namespace NereusSDR {

class HGauge;
class RadioModel;

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
    void    syncFromModel() override {}

    // Test seams (Section A - Task 7).
    QString deviceLabelTextForTesting()    const;
    QString nicknameLabelTextForTesting()  const;
    QString operateButtonTextForTesting()  const;
    void    clickOperateButtonForTesting();

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

signals:
    // Emitted when the user clicks the OPERATE/STANDBY toggle button.
    // requestedOperate=true means the user wants to enter OPERATE state.
    void operateToggled(bool requestedOperate);

    // Emitted when the user clicks one of the antenna buttons.
    void antennaRequested(RfKitAntenna::Type type, int number);

    // Right-click context menu signals (filled in Task 9).
    void navigationRequested(const QString& pageKey);
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

private:
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
    QHash<int, QString>      m_antennaLabels;
    QLabel*                  m_tunerStatusLabel{nullptr};
    QPushButton*             m_tuneBtn{nullptr};
    QPushButton*             m_bypassBtn{nullptr};
};

} // namespace NereusSDR
