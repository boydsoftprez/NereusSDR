// no-port-check: NereusSDR-original. The Rotor applet around the dial
// ported from Longpath (RotorDialWidget carries that port's header).

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/applets/RotorApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Rotor control plan, Task 6.
//
// Turns the station's antenna rotor, as the rotor mockup draws it
// (docs/architecture/2026-10-07-rotor-control-mockup.html): a status line,
// the dial (gui/widgets/RotorDialWidget), the heading readout with "to go"
// under it, CCW / STOP / CW held to turn, Down / Up on an az/el rotor,
// short and long path, the presets and a "Turn to" callsign box.
//
// It reads the rotor object (RadioModel::rotorModel()) and turns the rotor
// through RotorCommandSink, so it never asks whether this process or a
// remote Core runs the rotor. With no rotor, a rotor not connected or a
// Core too old the controls stay, greyed, with the reason.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 6).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "AppletWidget.h"
#include "models/RotorCommandSink.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace NereusSDR {

class RotorDialWidget;
namespace RotorLink { class RotorModel; }

class RotorApplet : public AppletWidget {
    Q_OBJECT
public:
    /// The contract's hold dead man: a held turn button repeats this often
    /// (the Core stops 750 ms after the last repeat).
    static constexpr int kHoldRepeatMs = 250;
    /// The Core calls a heading stale after this long without a reply
    /// (the contract's positionFresh).
    static constexpr int kStaleAfterMs = 1500;

    /// The rotor and the commands of `model` (RadioModel::rotorModel(),
    /// RadioModel as the RotorCommandSink).
    explicit RotorApplet(RadioModel* model, QWidget* parent = nullptr);
    /// For tests: a rotor object and a command sink of their own; `model`
    /// may be null.
    RotorApplet(RadioModel* model, RotorLink::RotorModel* rotor,
                RotorCommandSink* commands, QWidget* parent = nullptr);
    ~RotorApplet() override;

    QString appletId()    const override { return QStringLiteral("rotor"); }
    QString appletTitle() const override { return QStringLiteral("Rotor"); }
    void syncFromModel() override;

    RotorDialWidget* dial() const { return m_dial; }
    /// Why the controls are greyed, or empty while they work.
    QString disabledReason() const { return m_disabledReason; }
    bool longPath() const { return m_longPath; }

private:
    void buildUi();
    void bindRotor();
    void updateStatus();
    void updateReadout();
    void updateControls();
    void rebuildPresets();
    void updateMessage();

    void startHold(RotorCommandSink::Nudge direction);
    void endHold();
    void repeatHold();

    void sendTarget(double azimuthDeg, double elevationDeg, bool fromDial);
    void sendStop();
    void sendTurnToCall();
    // The Core's verdict on a command sent to a remote Core.
    void onCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    void noteSent(bool sent, const QString& reason, bool fromDial);

    QPushButton* makeButton(const QString& text, const QString& objectName);

    QPointer<RotorLink::RotorModel> m_rotor;
    RotorCommandSink* m_commands{nullptr};

    RotorDialWidget* m_dial{nullptr};
    QLabel* m_statusDot{nullptr};
    QLabel* m_statusText{nullptr};
    QLabel* m_heading{nullptr};
    QLabel* m_target{nullptr};
    QPushButton* m_ccw{nullptr};
    QPushButton* m_stop{nullptr};
    QPushButton* m_cw{nullptr};
    QWidget* m_elevationRow{nullptr};
    QPushButton* m_down{nullptr};
    QPushButton* m_up{nullptr};
    QPushButton* m_shortPath{nullptr};
    QPushButton* m_longPathButton{nullptr};
    QLabel* m_presetsLabel{nullptr};
    QWidget* m_presetsBox{nullptr};
    QGridLayout* m_presetsGrid{nullptr};
    QLabel* m_noPresets{nullptr};
    QLabel* m_turnToLabel{nullptr};
    QLineEdit* m_call{nullptr};
    QPushButton* m_turnTo{nullptr};
    QLabel* m_reason{nullptr};

    QString m_disabledReason;
    QString m_message;            // the last refusal, until the next command
    QString m_presetsShown;
    bool m_longPath{false};
    bool m_controlsEnabled{false};

    // The hold dead man.
    QTimer m_holdTimer;
    bool m_holding{false};
    RotorCommandSink::Nudge m_holdDirection{RotorCommandSink::Nudge::Cw};

    // A command waiting on a remote Core's verdict.
    quint32 m_pendingCommandId{0};
    bool m_pendingFromDial{false};

    // How long ago the rotor last answered (for "Last heard Ns ago").
    QElapsedTimer m_lastFresh;
    bool m_seenStale{false};
    qint64 m_staleOffsetMs{0};
    QTimer m_staleTicker;
};

} // namespace NereusSDR
