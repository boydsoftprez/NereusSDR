// src/gui/applets/CatApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;
class QLineEdit;
class QComboBox;

namespace NereusSDR {

// CAT / rigctld / TCI control interfaces.
// NYI — Phase 3K (CAT/rigctld — 4-channel rigctld, TCP CAT server).
class CatApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CatApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("cat"); }
    QString appletTitle() const override { return QStringLiteral("CAT"); }
    void    syncFromModel() override;

private:
    // rigctld row: enable button + 4 status LEDs (A/B/C/D)
    QPushButton* m_rigctldBtn    = nullptr;  // green toggle
    QLabel*      m_rigctldLed[4] = {};       // A/B/C/D status

    // TCP CAT row: enable button + port field
    QPushButton* m_tcpCatBtn     = nullptr;  // green toggle
    QLineEdit*   m_tcpCatPort    = nullptr;  // "4532", fixedWidth 50

    // TCI row: enable button + port field + status LED
    QPushButton* m_tciBtn        = nullptr;  // green toggle
    QLineEdit*   m_tciPort       = nullptr;  // "40001"
    QLabel*      m_tciLed        = nullptr;

    // Active connections readout
    QLabel*      m_connectLabel  = nullptr;

    // Serial port combo
    QComboBox*   m_serialCombo   = nullptr;
};

} // namespace NereusSDR
