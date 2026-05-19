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
//   Disconnect / Reconnect        -> connectionToggleRequested()
//   Copy diagnostics to clipboard -> diagnosticsCopyRequested()
//
// From AetherSDR src/gui/AmpApplet.h [@0cd4559]
class AmpApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit AmpApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("amp"); }
    QString appletTitle() const override { return QStringLiteral("AMP"); }
    void    syncFromModel() override {}

    // Test seam: returns a heap-allocated QMenu* without exec()-ing it.
    // Caller owns the returned menu; delete or deleteLater() as needed.
    // Same pattern as SMeterWidget::buildContextMenuForTesting() (Task 38,
    // commit 067d2d5b).
    QMenu* buildContextMenuForTesting() { return buildContextMenu(this); }

signals:
    // Emitted when the user clicks the OPERATE/STANDBY button.
    // requestedOperate=true means the user wants to enter OPERATE state.
    // From AetherSDR src/gui/AmpApplet.h:26 [@0cd4559]
    void operateToggled(bool requestedOperate);

    // Phase 3P-II Phase 4 Task 88: right-click context menu signals.

    // Emitted when "Open PGXL Advanced..." is triggered.
    // pageKey is "pgxlAdvanced"; MainWindow::openSetup() is the handler.
    void navigationRequested(const QString& pageKey);

    // Emitted when "Disconnect" / "Reconnect" is triggered.
    // MainWindow should call pgxlConnection()->disconnectFromPgxl() or
    // reconnect depending on current state.
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

    // Phase 3P-II Phase 4 Task 88: update the connected flag so the
    // context menu shows "Disconnect" vs "Reconnect" appropriately.
    void setPgxlConnected(bool connected);

protected:
    // Phase 3P-II Phase 4 Task 88: right-click context menu.
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
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
};

} // namespace NereusSDR
