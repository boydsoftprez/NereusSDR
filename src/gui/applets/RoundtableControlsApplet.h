// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/applets/RoundtableControlsApplet.h  (NereusSDR)
// =================================================================
//
// A small panel for the two Roundtable controls that otherwise live on
// Spot Hub, on one line. A gap after Start/Stop keeps Hide your status
// apart from the button. The words only name those controls.
//
// Start and Stop are the Spot Hub buttons (startRequested / stopRequested).
// Hide your status is RoundtableModel::setHidden, the same write Spot Hub
// makes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-25: Created. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
//   2026-11-25: The panel is named Roundtable controls (simple). Brian Sutton
//               (W4KZO), AI-assisted via xAI Grok.
//   2026-11-26: NEREUS_GUI_EXPORT; Start and Hide your status grey with a
//               reason when Roundtable can't run here (setStationAvailable,
//               as Spot Hub does), and a line under them says why a start
//               or a hide was refused (setSourceHost). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code / Opus 5.5.
// =================================================================

#pragma once

#include "AppletWidget.h"
#include "gui/NereusGuiExport.h"
#include "models/RoundtableModel.h"

#include <QPointer>

class QCheckBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class SpotSourceHost;

class NEREUS_GUI_EXPORT RoundtableControlsApplet : public AppletWidget {
    Q_OBJECT

public:
    /// The Roundtable of `model` (RadioModel::roundtableModel()).
    explicit RoundtableControlsApplet(RadioModel* model, QWidget* parent = nullptr);
    /// For tests: a Roundtable of its own; `model` may be null.
    RoundtableControlsApplet(RadioModel* model, RoundtableModel* roundtable, QWidget* parent);
    ~RoundtableControlsApplet() override;

    QString appletId() const override { return QStringLiteral("roundtable_controls"); }
    QString appletTitle() const override { return QStringLiteral("Roundtable controls (simple)"); }
    void syncFromModel() override;

    QPushButton* startButton() const { return m_start; }
    QCheckBox* hideBox() const { return m_hide; }
    /// The line under the controls: why Roundtable did not start, or why
    /// a hide was refused. Hidden while there is nothing to say.
    QLabel* reasonLabel() const { return m_reason; }

    /// Whether this window can ask for Start and Hide your status, as Spot
    /// Hub's Roundtable tab: false greys both, with `reason` as their
    /// tooltip (the Core doesn't run Roundtable, or its spot sources).
    void setStationAvailable(bool available, const QString& reason);
    /// The spot source host Start and Stop go to: its Roundtable text and
    /// refusals ("Enter your callsign in Spot Hub first.") show on the
    /// reason line. Not owned.
    void setSourceHost(SpotSourceHost* host);

signals:
    /// Spot Hub's Start. The owner asks the spot source host to start.
    void startRequested();
    /// Spot Hub's Stop.
    void stopRequested();

private:
    void build();
    void refresh();
    bool running() const;

    QPointer<RoundtableModel> m_roundtable;
    QPointer<SpotSourceHost> m_host;
    QPushButton* m_start = nullptr;
    QCheckBox* m_hide = nullptr;
    QLabel* m_reason = nullptr;
    bool m_available = true;
    QString m_unavailableReason;
    QString m_refusal;
};

} // namespace NereusSDR
