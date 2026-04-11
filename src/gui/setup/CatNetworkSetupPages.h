#pragma once

#include "gui/SetupPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// CAT > Serial Ports
// Four identical port sections, each with: port combo, baud combo,
// enable toggle, and status label. All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatSerialPortsPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatSerialPortsPage(QWidget* parent = nullptr);

private:
    struct PortRow {
        QComboBox*  portCombo{nullptr};
        QComboBox*  baudCombo{nullptr};
        QCheckBox*  enableCheck{nullptr};
        QLabel*     statusLabel{nullptr};
    };

    PortRow m_ports[4];

    void buildUI();
};

// ---------------------------------------------------------------------------
// CAT > TCI Server
// Enable toggle, bind IP, port spinner, status label, connected clients label.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatTciServerPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatTciServerPage(QWidget* parent = nullptr);

private:
    QCheckBox* m_enableCheck{nullptr};
    QLineEdit* m_bindIpEdit{nullptr};
    QSpinBox*  m_portSpin{nullptr};
    QLabel*    m_statusLabel{nullptr};
    QLabel*    m_clientsLabel{nullptr};

    void buildUI();
};

// ---------------------------------------------------------------------------
// CAT > TCP/IP CAT
// Enable toggle, bind IP, port spinner, status label.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatTcpIpPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatTcpIpPage(QWidget* parent = nullptr);

private:
    QCheckBox* m_enableCheck{nullptr};
    QLineEdit* m_bindIpEdit{nullptr};
    QSpinBox*  m_portSpin{nullptr};
    QLabel*    m_statusLabel{nullptr};

    void buildUI();
};

// ---------------------------------------------------------------------------
// CAT > MIDI Control
// Enable toggle, device combo, mapping table placeholder, learn button.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatMidiControlPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatMidiControlPage(QWidget* parent = nullptr);

private:
    QCheckBox*  m_enableCheck{nullptr};
    QComboBox*  m_deviceCombo{nullptr};
    QLabel*     m_mappingLabel{nullptr};
    QPushButton* m_learnButton{nullptr};

    void buildUI();
};

} // namespace NereusSDR
