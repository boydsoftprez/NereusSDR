// no-port-check: NereusSDR-original. The antenna rotor's setup page.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/setup/RotorSetupPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See RotorSetupPage.h.
//
// The Hamlib model list and the baud rates are core/RotorModels.h (after
// Longpath). The help text's Easy Rotor Control facts are the maker's
// setup example (GS-232B, 9600 baud, 8N1; ERC SMD USB V4.3 Instructions,
// section 7, as the design cites it) and the bench capture of JJ's rotor
// (a south end stop and 450 degrees of travel, tests/data/rotor/). The
// defaults are the contract's, not these facts.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 7).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Bench fix: the page scrolls, so the
//                                    Settings window no longer squeezes its
//                                    rows; the status line shows the live
//                                    heading. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Final review I3: on screen, the page
//                                    has the rotor's computer read its
//                                    ports. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "RotorSetupPage.h"

#include "core/RotorHeading.h"
#include "core/RotorModels.h"
#include "core/StationRotorController.h"
#include "gui/OperatorReasonText.h"
#include "models/RadioModel.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

// The `rotor` object (NereusSDR::RotorModel is the Hamlib model list).
using LinkRotor = RotorLink::RotorModel;

namespace {

constexpr const char* kNoteStyle = "color:#9aa5b1; font-size:11px;";
constexpr const char* kGoodStyle = "color:#7ec850; font-size:11px; font-weight:bold;";
constexpr const char* kWarnStyle = "color:#ffcc66; font-size:11px; font-weight:bold;";

// The Hamlib model combo's last entry: a model number typed in the box.
constexpr int kOtherModel = -1;

bool usesSerialPort(int driver)
{
    return driver == static_cast<int>(LinkRotor::Driver::Gs232a)
        || driver == static_cast<int>(LinkRotor::Driver::Gs232b)
        || driver == static_cast<int>(LinkRotor::Driver::RotctldStarted);
}

QLabel* noteLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setStyleSheet(QString::fromLatin1(kNoteStyle));
    return label;
}

void selectData(QComboBox* combo, const QVariant& value)
{
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

} // namespace

RotorSetupPage::RotorSetupPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_commands(model)
    , m_rotor(model ? model->rotorModel() : nullptr)
{
    // Bench fix: the page is taller than the Settings window, which gives a
    // page only its own height. Without a scroll area the window squeezed
    // every row (fields a few pixels tall, help text cut off); in one, each
    // row keeps its natural height and long help text wraps in full.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("rotorSetupScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);
    auto* content = new QWidget(scroll);
    content->setAutoFillBackground(false);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    m_availability = new QLabel(this);
    m_availability->setObjectName(QStringLiteral("rotorSetupAvailability"));
    m_availability->setWordWrap(true);
    m_availability->setTextFormat(Qt::PlainText);
    root->addWidget(m_availability);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("rotorSetupStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    root->addWidget(m_status);

    root->addWidget(buildConnectionGroup());
    root->addWidget(buildRotorGroup());

    m_save = new QPushButton(tr("Save and connect"), this);
    m_save->setObjectName(QStringLiteral("rotorSetupSave"));
    m_save->setToolTip(tr("Save this setup with the rotor and connect to it. With no rotor "
                          "chosen, the rotor is disconnected and its setup forgotten."));
    connect(m_save, &QPushButton::clicked, this, &RotorSetupPage::onSaveClicked);
    auto* saveRow = new QHBoxLayout();
    saveRow->addWidget(m_save);
    saveRow->addStretch();
    root->addLayout(saveRow);

    root->addWidget(buildPresetsGroup());

    m_message = new QLabel(this);
    m_message->setObjectName(QStringLiteral("rotorSetupMessage"));
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setStyleSheet(QString::fromLatin1(kWarnStyle));
    m_message->setVisible(false);
    root->addWidget(m_message);

    root->addStretch();

    // Any change the operator makes stays until it is sent and answered.
    const auto touchSetup = [this] {
        if (!m_filling) {
            m_setupTouched = true;
        }
    };
    for (QComboBox* combo : {m_driver, m_serialPort, m_baud, m_hamlib, m_axes, m_endStop,
                             m_range}) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, touchSetup);
    }
    connect(m_host, &QLineEdit::textEdited, this, touchSetup);
    connect(m_port, QOverload<int>::of(&QSpinBox::valueChanged), this, touchSetup);
    connect(m_hamlibModel, QOverload<int>::of(&QSpinBox::valueChanged), this, touchSetup);
    connect(m_offset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, touchSetup);

    connect(m_driver, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RotorSetupPage::refreshDriverFields);

    if (m_rotor) {
        connect(m_rotor, &LinkRotor::stateChanged, this, [this] {
            refreshFromRotor();
            refreshStatus();
        });
        // Bench fix: the live heading is on the status line.
        connect(m_rotor, &LinkRotor::positionChanged, this, &RotorSetupPage::refreshStatus);
    }
    if (m_model) {
        connect(m_model, &RadioModel::stationLinkStateChanged, this,
                &RotorSetupPage::refreshAvailability);
        connect(m_model, &RadioModel::stationCommandFinished, this,
                &RotorSetupPage::onCommandFinished);
    }

    refreshFromRotor();
    refreshStatus();
    refreshAvailability();
}

RotorSetupPage::~RotorSetupPage()
{
    setViewOpen(false);
}

void RotorSetupPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    setViewOpen(true);
}

void RotorSetupPage::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    setViewOpen(false);
}

void RotorSetupPage::setViewOpen(bool open)
{
    if (open == m_viewOpen) {
        return;
    }
    if (open) {
        m_viewModel = m_model;
    }
    m_viewOpen = open;
    if (m_viewModel) {
        m_viewModel->setRotorSetupViewOpen(open);
    }
}

QGroupBox* RotorSetupPage::buildConnectionGroup()
{
    m_connectionBox = new QGroupBox(tr("Connection"), this);
    auto* form = new QFormLayout(m_connectionBox);

    m_driver = new QComboBox(m_connectionBox);
    m_driver->setObjectName(QStringLiteral("rotorSetupDriver"));
    m_driver->addItem(tr("No rotor"), static_cast<int>(LinkRotor::Driver::None));
    m_driver->addItem(tr("Yaesu GS-232A, serial port"),
                      static_cast<int>(LinkRotor::Driver::Gs232a));
    m_driver->addItem(tr("Yaesu GS-232B, serial port"),
                      static_cast<int>(LinkRotor::Driver::Gs232b));
    m_driver->addItem(tr("Hamlib rotctld, already running"),
                      static_cast<int>(LinkRotor::Driver::Rotctld));
    m_driver->addItem(tr("Hamlib rotctld, started by the Core"),
                      static_cast<int>(LinkRotor::Driver::RotctldStarted));
    form->addRow(tr("Controller:"), m_driver);

    m_driverNote = noteLabel(m_connectionBox);
    m_driverNote->setText(tr("An Easy Rotor Control (ERC) box speaks Yaesu GS-232B. Its maker's "
                             "setup uses 9600 baud, 8 data bits, no parity, 1 stop bit."));
    form->addRow(QString(), m_driverNote);

    m_serialPort = new QComboBox(m_connectionBox);
    m_serialPort->setObjectName(QStringLiteral("rotorSetupSerialPort"));
    m_serialPort->setToolTip(tr("The serial ports on the computer that runs the rotor."));
    form->addRow(tr("Serial port:"), m_serialPort);
    m_serialNote = noteLabel(m_connectionBox);
    form->addRow(QString(), m_serialNote);

    m_baud = new QComboBox(m_connectionBox);
    m_baud->setObjectName(QStringLiteral("rotorSetupBaud"));
    for (int baud : commonRotorBauds()) {
        m_baud->addItem(QString::number(baud), baud);
    }
    form->addRow(tr("Baud:"), m_baud);

    m_host = new QLineEdit(m_connectionBox);
    m_host->setObjectName(QStringLiteral("rotorSetupHost"));
    m_host->setPlaceholderText(QStringLiteral("127.0.0.1"));
    form->addRow(tr("Host:"), m_host);

    m_port = new QSpinBox(m_connectionBox);
    m_port->setObjectName(QStringLiteral("rotorSetupPort"));
    m_port->setRange(1, 65535);
    m_port->setValue(4533);
    form->addRow(tr("Port:"), m_port);

    m_hamlib = new QComboBox(m_connectionBox);
    m_hamlib->setObjectName(QStringLiteral("rotorSetupHamlibModel"));
    for (const NereusSDR::RotorModel& entry : commonRotorModels()) {
        m_hamlib->addItem(QStringLiteral("%1 (%2)").arg(entry.name).arg(entry.hamlibId),
                          entry.hamlibId);
        m_hamlib->setItemData(m_hamlib->count() - 1, entry.note, Qt::ToolTipRole);
    }
    m_hamlib->addItem(tr("Another model number"), kOtherModel);
    form->addRow(tr("Hamlib model:"), m_hamlib);

    m_hamlibModel = new QSpinBox(m_connectionBox);
    m_hamlibModel->setObjectName(QStringLiteral("rotorSetupHamlibNumber"));
    m_hamlibModel->setRange(1, 999999);
    m_hamlibModel->setValue(commonRotorModels().constFirst().hamlibId);
    form->addRow(tr("Model number:"), m_hamlibModel);
    m_hamlibNote = noteLabel(m_connectionBox);
    form->addRow(QString(), m_hamlibNote);

    // The picker and the number follow each other.
    connect(m_hamlib, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        const int model = m_hamlib->currentData().toInt();
        if (model != kOtherModel) {
            const QSignalBlocker block(m_hamlibModel);
            m_hamlibModel->setValue(model);
        }
        refreshDriverFields();
    });
    connect(m_hamlibModel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int model) {
        const QSignalBlocker block(m_hamlib);
        const int index = m_hamlib->findData(model);
        m_hamlib->setCurrentIndex(index >= 0 ? index : m_hamlib->findData(kOtherModel));
        refreshDriverFields();
    });
    return m_connectionBox;
}

QGroupBox* RotorSetupPage::buildRotorGroup()
{
    m_rotorBox = new QGroupBox(tr("Rotor"), this);
    auto* form = new QFormLayout(m_rotorBox);

    m_axes = new QComboBox(m_rotorBox);
    m_axes->setObjectName(QStringLiteral("rotorSetupAxes"));
    m_axes->addItem(tr("Azimuth only"), static_cast<int>(LinkRotor::Axes::Azimuth));
    m_axes->addItem(tr("Azimuth and elevation"),
                    static_cast<int>(LinkRotor::Axes::AzimuthElevation));
    form->addRow(tr("Turns in:"), m_axes);

    m_endStop = new QComboBox(m_rotorBox);
    m_endStop->setObjectName(QStringLiteral("rotorSetupEndStop"));
    m_endStop->addItem(tr("None, turns all the way round"),
                       static_cast<int>(LinkRotor::EndStop::None));
    m_endStop->addItem(tr("North"), static_cast<int>(LinkRotor::EndStop::North));
    m_endStop->addItem(tr("South"), static_cast<int>(LinkRotor::EndStop::South));
    form->addRow(tr("End stop:"), m_endStop);

    m_range = new QComboBox(m_rotorBox);
    m_range->setObjectName(QStringLiteral("rotorSetupRange"));
    m_range->addItem(tr("360 degrees"), 360);
    m_range->addItem(tr("450 degrees, with overlap"), 450);
    form->addRow(tr("Travel:"), m_range);

    auto* travelNote = noteLabel(m_rotorBox);
    travelNote->setText(tr("The end stop is where the rotor stops turning counter-clockwise. "
                           "Choose 450 degrees for a rotor that turns past a full circle, for "
                           "example a Yaesu with its end stop at south. The Rotor panel then shows "
                           "the way round the rotor will really turn."));
    form->addRow(QString(), travelNote);

    m_offset = new QDoubleSpinBox(m_rotorBox);
    m_offset->setObjectName(QStringLiteral("rotorSetupOffset"));
    m_offset->setRange(-180.0, 180.0);
    m_offset->setDecimals(1);
    m_offset->setSingleStep(1.0);
    m_offset->setSuffix(QStringLiteral(" °"));
    m_offset->setToolTip(tr("Added to the heading the rotor reads, when its pointer is off."));
    form->addRow(tr("Heading offset:"), m_offset);
    return m_rotorBox;
}

QGroupBox* RotorSetupPage::buildPresetsGroup()
{
    m_presetsBox = new QGroupBox(tr("Presets"), this);
    auto* lay = new QVBoxLayout(m_presetsBox);

    auto* note = noteLabel(m_presetsBox);
    note->setText(tr("Headings the Rotor panel offers as one-tap buttons, 0 to 360 degrees."));
    lay->addWidget(note);

    m_presets = new QTableWidget(0, 2, m_presetsBox);
    m_presets->setObjectName(QStringLiteral("rotorSetupPresets"));
    m_presets->setHorizontalHeaderLabels({tr("Name"), tr("Heading")});
    m_presets->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_presets->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_presets->verticalHeader()->setVisible(false);
    m_presets->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_presets->setMinimumHeight(140);
    lay->addWidget(m_presets);
    connect(m_presets, &QTableWidget::itemChanged, this, [this] {
        if (!m_filling) {
            m_presetsTouched = true;
        }
    });

    m_addPreset = new QPushButton(tr("Add"), m_presetsBox);
    m_removePreset = new QPushButton(tr("Remove"), m_presetsBox);
    m_savePresets = new QPushButton(tr("Save presets"), m_presetsBox);
    m_savePresets->setObjectName(QStringLiteral("rotorSetupSavePresets"));
    auto* row = new QHBoxLayout();
    row->addWidget(m_addPreset);
    row->addWidget(m_removePreset);
    row->addStretch();
    row->addWidget(m_savePresets);
    lay->addLayout(row);

    connect(m_addPreset, &QPushButton::clicked, this, [this] {
        const int r = m_presets->rowCount();
        m_presets->insertRow(r);
        m_presets->setItem(r, 0, new QTableWidgetItem());
        m_presets->setItem(r, 1, new QTableWidgetItem());
        m_presetsTouched = true;
        m_presets->setCurrentCell(r, 0);
    });
    connect(m_removePreset, &QPushButton::clicked, this, [this] {
        const int r = m_presets->currentRow();
        if (r >= 0) {
            m_presets->removeRow(r);
            m_presetsTouched = true;
        }
    });
    connect(m_savePresets, &QPushButton::clicked, this, &RotorSetupPage::onSavePresetsClicked);
    return m_presetsBox;
}

bool RotorSetupPage::isRemote() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

void RotorSetupPage::refreshSerialPorts()
{
    if (!m_rotor) {
        return;
    }
    // The ports of the computer that runs the rotor: in a remote window,
    // the Core's (the `rotor` object's serialPorts), never this one's.
    QStringList ports = m_rotor->serialPorts().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString keep = m_setupTouched ? m_serialPort->currentText() : m_rotor->serialPort();
    QStringList items = ports;
    if (!keep.isEmpty() && !items.contains(keep)) {
        // The saved port is gone from that computer: still shown, so the
        // setup reads as it is.
        items.prepend(keep);
    }
    QStringList current;
    for (int i = 0; i < m_serialPort->count(); ++i) {
        current.append(m_serialPort->itemText(i));
    }
    if (current != items) {
        const QSignalBlocker block(m_serialPort);
        m_serialPort->clear();
        m_serialPort->addItems(items);
    }
    {
        const QSignalBlocker block(m_serialPort);
        int index = keep.isEmpty() ? -1 : m_serialPort->findText(keep);
        if (index < 0 && m_serialPort->count() > 0) {
            index = 0;
        }
        m_serialPort->setCurrentIndex(index);
    }
    const QString where = isRemote() ? tr("the Core's computer") : tr("this computer");
    if (ports.isEmpty()) {
        m_serialNote->setText(tr("No serial ports were found on %1.").arg(where));
    } else if (!keep.isEmpty() && !ports.contains(keep)) {
        m_serialNote->setText(tr("%1 is not on %2 now.").arg(keep, where));
    } else {
        m_serialNote->setText(tr("Serial ports on %1.").arg(where));
    }
}

void RotorSetupPage::refreshFromRotor()
{
    if (!m_rotor) {
        return;
    }
    m_filling = true;
    refreshSerialPorts();
    if (!m_setupTouched) {
        const LinkRotor::State s = m_rotor->state();
        selectData(m_driver, static_cast<int>(s.driver));
        selectData(m_baud, s.baud);
        if (m_baud->findData(s.baud) < 0 && s.baud > 0) {
            m_baud->addItem(QString::number(s.baud), s.baud);
            selectData(m_baud, s.baud);
        }
        m_host->setText(s.host);
        if (s.port >= 1) {
            m_port->setValue(s.port);
        }
        if (s.hamlibModel > 0) {
            selectHamlibModel(s.hamlibModel);
        }
        selectData(m_axes, static_cast<int>(s.axes));
        selectData(m_endStop, static_cast<int>(s.endStop));
        selectData(m_range, s.rangeDeg);
        m_offset->setValue(s.offsetDeg);
    }
    m_filling = false;
    refreshPresetsFromRotor();
    refreshDriverFields();
}

void RotorSetupPage::selectHamlibModel(int model)
{
    const QSignalBlocker blockSpin(m_hamlibModel);
    const QSignalBlocker blockCombo(m_hamlib);
    m_hamlibModel->setValue(model);
    const int index = m_hamlib->findData(model);
    m_hamlib->setCurrentIndex(index >= 0 ? index : m_hamlib->findData(kOtherModel));
}

void RotorSetupPage::refreshPresetsFromRotor()
{
    if (!m_rotor || m_presetsTouched) {
        return;
    }
    m_filling = true;
    m_presets->setRowCount(0);
    for (const QString& line : m_rotor->presets().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const qsizetype tab = line.lastIndexOf(QLatin1Char('\t'));
        const int r = m_presets->rowCount();
        m_presets->insertRow(r);
        m_presets->setItem(r, 0, new QTableWidgetItem(tab >= 0 ? line.left(tab) : QString()));
        m_presets->setItem(r, 1, new QTableWidgetItem(tab >= 0 ? line.mid(tab + 1) : line));
    }
    m_filling = false;
}

void RotorSetupPage::refreshDriverFields()
{
    const int driver = m_driver->currentData().toInt();
    const bool serial = usesSerialPort(driver);
    const bool network = driver == static_cast<int>(LinkRotor::Driver::Rotctld);
    const bool started = driver == static_cast<int>(LinkRotor::Driver::RotctldStarted);
    const bool any = driver != static_cast<int>(LinkRotor::Driver::None);
    // Disabled, never hidden: a field this driver does not use is greyed.
    m_serialPort->setEnabled(serial);
    m_baud->setEnabled(serial);
    m_host->setEnabled(network);
    m_port->setEnabled(network || started);
    m_hamlib->setEnabled(started);
    m_hamlibModel->setEnabled(started);
    m_axes->setEnabled(any);
    m_endStop->setEnabled(any);
    m_range->setEnabled(any);
    m_offset->setEnabled(any);
    m_port->setToolTip(started ? tr("The port rotctld listens on, on the Core's own computer.")
                               : QString());

    QString hamlibNote;
    if (started) {
        const int index = m_hamlib->currentIndex();
        hamlibNote = index >= 0 ? m_hamlib->itemData(index, Qt::ToolTipRole).toString()
                                : QString();
        if (m_rotor && !m_rotor->rotctldAvailable()) {
            hamlibNote = StationRotorController::rotctldMissingReason();
        }
    }
    m_hamlibNote->setText(hamlibNote);
    m_hamlibNote->setVisible(!hamlibNote.isEmpty());
}

void RotorSetupPage::refreshStatus()
{
    if (!m_rotor) {
        m_status->setText(QString());
        return;
    }
    using Phase = TunerModel::ConnectionPhase;
    const LinkRotor::State s = m_rotor->state();
    QString text;
    if (s.driver == LinkRotor::Driver::None) {
        text = tr("No rotor is set up.");
    } else {
        const QString label = s.label.isEmpty() ? tr("The rotor") : s.label;
        switch (s.connectionPhase) {
        case Phase::Disabled:
        case Phase::Disconnected: text = tr("%1: disconnected").arg(label); break;
        case Phase::Discovering:
        case Phase::Connecting:
        case Phase::Identifying: text = tr("%1: connecting").arg(label); break;
        case Phase::Retrying: text = tr("%1: trying again").arg(label); break;
        case Phase::Connected:
            // Bench fix: the heading the rotor reads, while it is fresh, so
            // the page shows the rotor answering (JJ had nowhere to see it).
            if (s.positionFresh && s.azimuthDeg >= 0.0) {
                const int heading = static_cast<int>(std::lround(s.azimuthDeg)) % 360;
                text = tr("%1: connected, heading %2°").arg(label).arg(heading);
            } else {
                text = tr("%1: connected").arg(label);
            }
            break;
        case Phase::Error:
            text = tr("%1: %2").arg(label,
                                    OperatorReasonText::forDisplay(s.connectionError));
            break;
        }
        if (!s.fault.isEmpty()) {
            text += QStringLiteral("\n") + OperatorReasonText::forDisplay(s.fault);
        }
    }
    m_status->setText(text);
}

void RotorSetupPage::refreshAvailability()
{
    QString reason;
    const bool available = m_commands && m_commands->rotorControlAvailable(&reason);
    // Disabled, never hidden: every control stays, greyed, with the reason.
    for (QWidget* w : std::initializer_list<QWidget*>{m_connectionBox, m_rotorBox, m_presetsBox,
                                                      m_save}) {
        w->setEnabled(available);
        w->setToolTip(available ? QString() : reason);
    }
    if (available) {
        m_save->setToolTip(tr("Save this setup with the rotor and connect to it. With no rotor "
                              "chosen, the rotor is disconnected and its setup forgotten."));
    }
    if (!available) {
        m_availability->setText(OperatorReasonText::forDisplay(reason));
        m_availability->setStyleSheet(QString::fromLatin1(kWarnStyle));
    } else if (isRemote()) {
        m_availability->setText(tr("The rotor is the Core's. Changes here are saved on the "
                                   "Core and its computer turns the rotor."));
        m_availability->setStyleSheet(QString::fromLatin1(kGoodStyle));
    } else {
        m_availability->setText(tr("This computer runs the rotor."));
        m_availability->setStyleSheet(QString::fromLatin1(kGoodStyle));
    }
}

void RotorSetupPage::onSaveClicked()
{
    if (!m_commands) {
        return;
    }
    RotorCommandSink::Setup setup;
    setup.driver = m_driver->currentData().toInt();
    setup.serialPort = usesSerialPort(setup.driver) ? m_serialPort->currentText() : QString();
    setup.baud = m_baud->currentData().toInt();
    setup.host = m_host->text().trimmed();
    setup.port = m_port->value();
    setup.hamlibModel =
        setup.driver == static_cast<int>(LinkRotor::Driver::RotctldStarted)
            ? m_hamlibModel->value() : 0;
    setup.axes = m_axes->currentData().toInt();
    setup.endStop = m_endStop->currentData().toInt();
    setup.rangeDeg = m_range->currentData().toInt();
    setup.offsetDeg = m_offset->value();

    QString reason;
    const bool sent = m_commands->requestConfigureRotor(setup, &reason);
    m_pendingSetupId = sent ? m_commands->lastRotorCommandId() : 0;
    if (!sent) {
        showMessage(OperatorReasonText::forDisplay(reason));
        return;
    }
    showMessage(QString());
    if (m_pendingSetupId != 0 && m_model) {
        // The Core's answer follows; its refusal is this page's to show
        // while the page is on screen.
        m_model->noteAccessoryRequestShownOnPage(m_pendingSetupId, this);
        return;
    }
    // This process's own rotor took it at once.
    m_setupTouched = false;
    refreshFromRotor();
}

void RotorSetupPage::onSavePresetsClicked()
{
    if (!m_commands) {
        return;
    }
    // The strict heading rule, as the Core applies it: an empty or
    // unreadable heading is refused, never taken as north.
    QStringList lines;
    for (int r = 0; r < m_presets->rowCount(); ++r) {
        const QTableWidgetItem* nameItem = m_presets->item(r, 0);
        const QTableWidgetItem* degItem = m_presets->item(r, 1);
        QString name = nameItem ? nameItem->text().trimmed() : QString();
        const QString degText = degItem ? degItem->text().trimmed() : QString();
        if (name.isEmpty() && degText.isEmpty()) {
            continue;
        }
        // A tab or line break would split the preset's line.
        name.replace(QLatin1Char('\t'), QLatin1Char(' '));
        name.replace(QLatin1Char('\n'), QLatin1Char(' '));
        name.remove(QLatin1Char('\r'));
        double deg = 0.0;
        if (!RotorHeading::parse(degText, &deg)) {
            bool ok = false;
            const double v = degText.toDouble(&ok);
            showMessage(ok && std::isfinite(v) ? StationRotorController::outOfRangeReason()
                                               : StationRotorController::notANumberReason());
            m_presets->setCurrentCell(r, 1);
            return;
        }
        lines.append(name + QLatin1Char('\t') + QString::number(deg, 'g', 10));
    }
    QString reason;
    const bool sent = m_commands->requestRotorPresets(lines.join(QLatin1Char('\n')), &reason);
    m_pendingPresetsId = sent ? m_commands->lastRotorCommandId() : 0;
    if (!sent) {
        showMessage(OperatorReasonText::forDisplay(reason));
        return;
    }
    showMessage(QString());
    if (m_pendingPresetsId != 0 && m_model) {
        m_model->noteAccessoryRequestShownOnPage(m_pendingPresetsId, this);
        return;
    }
    m_presetsTouched = false;
    refreshPresetsFromRotor();
}

void RotorSetupPage::onCommandFinished(quint32 commandId, bool accepted, const QString& reason)
{
    if (commandId == 0) {
        return;
    }
    if (commandId == m_pendingSetupId) {
        m_pendingSetupId = 0;
        if (accepted) {
            // The Core has it; the fields follow the rotor again.
            m_setupTouched = false;
            refreshFromRotor();
        }
    } else if (commandId == m_pendingPresetsId) {
        m_pendingPresetsId = 0;
        if (accepted) {
            m_presetsTouched = false;
            refreshPresetsFromRotor();
        }
    } else {
        return;
    }
    if (!accepted) {
        showMessage(OperatorReasonText::forDisplay(
            reason.isEmpty() ? tr("The Core refused the request without giving a reason.")
                             : reason));
    }
}

void RotorSetupPage::showMessage(const QString& text)
{
    m_message->setText(text);
    m_message->setVisible(!text.isEmpty());
}

QString RotorSetupPage::statusTextForTesting() const
{
    return m_status->text();
}

QString RotorSetupPage::messageTextForTesting() const
{
    return m_message->isVisibleTo(this) ? m_message->text() : QString();
}

QString RotorSetupPage::availabilityTextForTesting() const
{
    return m_availability->text();
}

} // namespace NereusSDR
