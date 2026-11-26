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
// =================================================================

#pragma once

#include "AppletWidget.h"
#include "models/RoundtableModel.h"

#include <QPointer>

class QCheckBox;
class QPushButton;

namespace NereusSDR {

class RoundtableControlsApplet : public AppletWidget {
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
    QPushButton* m_start = nullptr;
    QCheckBox* m_hide = nullptr;
};

} // namespace NereusSDR
