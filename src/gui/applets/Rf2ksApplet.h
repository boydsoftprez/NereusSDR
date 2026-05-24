// =================================================================
// src/gui/applets/Rf2ksApplet.h  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   Layout patterns from src/gui/applets/AmpApplet.{h,cpp} (which is
//   an AetherSDR port). The RF-Kit-specific content is original.
// =================================================================
#pragma once
#include "AppletWidget.h"

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
// Sections B-D (Tasks 8-9): gauges, antenna selector, tuner controls,
// right-click context menu.
//
// Layout patterns from AmpApplet.{h,cpp} (AetherSDR port, GPLv3).
class Rf2ksApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit Rf2ksApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("RfKit"); }
    QString appletTitle() const override { return QStringLiteral("RF-Kit RF2K-S"); }
    void    syncFromModel() override {}

    // Test seams.
    QString deviceLabelTextForTesting()    const;
    QString nicknameLabelTextForTesting()  const;
    QString operateButtonTextForTesting()  const;
    void    clickOperateButtonForTesting();

signals:
    // Emitted when the user clicks the OPERATE/STANDBY toggle button.
    // requestedOperate=true means the user wants to enter OPERATE state.
    void operateToggled(bool requestedOperate);

    // Right-click context menu signals (filled in Task 9).
    void navigationRequested(const QString& pageKey);
    void connectionToggleRequested();
    void diagnosticsCopyRequested();

public slots:
    void setNicknameAndVersion(const QString& nickname, const QString& version);
    void setOperateMode(const QString& mode);   // "OPERATE" or "STANDBY"
    void setConnectedState(bool connected);

private:
    QLabel*      m_deviceLabel{nullptr};
    QLabel*      m_nicknameLabel{nullptr};
    QPushButton* m_operateBtn{nullptr};
    QLabel*      m_statusDot{nullptr};
    QString      m_operateMode;
    bool         m_connected{false};
};

} // namespace NereusSDR
