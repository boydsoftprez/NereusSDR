// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 11.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 18: its list and cell helpers
//                moved to HotKeySetupCommon.h for the Knobs page.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/setup/MidiSetupPage.h"

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/midi/MidiInput.h"
#include "gui/setup/HotKeySetupCommon.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <utility>

namespace NereusSDR {

using namespace HotKeySetup;

namespace {

constexpr int kColumnControl = 0;
constexpr int kColumnKind = 1;
constexpr int kColumnAction = 2;
constexpr int kColumnDown = 3;
constexpr int kColumnActsOn = 4;
constexpr int kColumnClear = 5;
constexpr int kColumnCount = 6;
constexpr int kListHeight = 280;
constexpr int kRowHeight = 30;

QString noHostReason()
{
    return QStringLiteral("MIDI belongs to a radio window, and this page has none.");
}

QString noSupportReason()
{
    return QStringLiteral("This build has no MIDI support.");
}

QString noDeviceReason()
{
    return QStringLiteral("No MIDI device plugged in.");
}

QString chooseDeviceReason()
{
    return QStringLiteral("Choose a MIDI device.");
}

QString deviceGoneReason()
{
    return QStringLiteral("That MIDI device is not plugged in.");
}

QString learnPrompt()
{
    return QStringLiteral("Pick an action, then move a control.");
}

QString defaultProfileName()
{
    return QStringLiteral("Default");
}

// ch1.cc7 in the words the operator reads.
QString controlName(const QString& key)
{
    static const QRegularExpression kPattern(QStringLiteral("^ch(\\d+)\\.(note|cc|bend)(\\d*)$"));
    const QRegularExpressionMatch match = kPattern.match(key);
    if (!match.hasMatch()) {
        return key;
    }
    const QString channel = match.captured(1);
    const QString type = match.captured(2);
    if (type == QLatin1String("note")) {
        return QStringLiteral("Ch %1 Note %2").arg(channel, match.captured(3));
    }
    if (type == QLatin1String("cc")) {
        return QStringLiteral("Ch %1 CC %2").arg(channel, match.captured(3));
    }
    return QStringLiteral("Ch %1 Pitch bend").arg(channel);
}

QString kindName(MidiControlKind kind)
{
    switch (kind) {
    case MidiControlKind::Button:
        return QStringLiteral("Button");
    case MidiControlKind::AbsoluteKnob:
        return QStringLiteral("Fader");
    case MidiControlKind::RelativeKnob:
        return QStringLiteral("Endless knob");
    }
    return {};
}

QString kindHelp(MidiControlKind kind)
{
    switch (kind) {
    case MidiControlKind::Button:
        return QStringLiteral("Runs its action when pressed.");
    case MidiControlKind::AbsoluteKnob:
        return QStringLiteral("A fader or a knob with stops: it sends where it is.");
    case MidiControlKind::RelativeKnob:
        return QStringLiteral("A knob that turns forever: it sends which way it moved.");
    }
    return {};
}

// Why this control can't be that kind; empty when it can. A note only presses,
// and a bend always sends where the wheel is.
QString kindRefusal(const QString& key, MidiControlKind kind)
{
    if (key.contains(QLatin1String(".note")) && kind != MidiControlKind::Button) {
        return QStringLiteral("A note can only be a button.");
    }
    if (key.endsWith(QLatin1String(".bend")) && kind != MidiControlKind::AbsoluteKnob) {
        return QStringLiteral("Pitch bend sends where the wheel is, so it can only be a fader.");
    }
    return {};
}

} // namespace

MidiSetupPage::MidiSetupPage(ControllerHost* host, HotKeyBindings* bindings, QWidget* parent)
    : SetupPage(QStringLiteral("MIDI"), parent)
    , m_host(host)
    , m_bindings(bindings)
    , m_supported(MidiInput::supported())
{
    buildController();
    buildControls();

    m_deviceTimer = new QTimer(this);
    m_deviceTimer->setInterval(kDeviceCheckMs);
    connect(m_deviceTimer, &QTimer::timeout, this, &MidiSetupPage::refreshDevices);

    if (m_host) {
        MidiInput* const midi = m_host->midi();
        connect(midi, &MidiInput::learned, this, &MidiSetupPage::onLearned, Qt::QueuedConnection);
        connect(midi, &MidiInput::connectionChanged, this,
                [this](bool connected, const QString& reason) {
                    m_connectionReason = connected ? QString() : reason;
                    applyAvailability();
                },
                Qt::QueuedConnection);
        connect(midi, &MidiInput::controlEvent, this,
                [this](const QString& controlKey, MidiControlKind, int value, int) {
                    if (value > 0) {
                        m_last->setText(QStringLiteral("Last control: %1").arg(controlName(controlKey)));
                    }
                },
                Qt::QueuedConnection);
    }
    if (m_bindings) {
        connect(m_bindings.data(), &HotKeyBindings::enabledChanged, this, &MidiSetupPage::applyAvailability);
    }

    loadActiveProfile();
    refreshDevices();
}

MidiSetupPage::~MidiSetupPage()
{
    endLearn();
}

void MidiSetupPage::setUnavailableReasonProvider(ReasonProvider provider)
{
    m_reason = std::move(provider);
    refreshAvailability();
}

void MidiSetupPage::refreshAvailability()
{
    markUnavailableActions(m_learn);
    for (Row& row : m_rows) {
        markUnavailableActions(row.action);
        markUnavailableActions(row.down);
    }
    applyAvailability();
}

void MidiSetupPage::buildController()
{
    QGroupBox* const group = addSection(QStringLiteral("Controller (untested)"));
    group->setObjectName(QStringLiteral("midiControllerGroup"));
    auto* const layout = qobject_cast<QVBoxLayout*>(group->layout());

    auto* const deviceRow = new QHBoxLayout;
    deviceRow->setSpacing(6);
    deviceRow->addWidget(new QLabel(QStringLiteral("Device:"), group));
    m_device = new QComboBox(group);
    m_device->setObjectName(QStringLiteral("midiDevice"));
    m_device->setProperty("nereusSetupId", QStringLiteral("keys.midi.device"));
    m_device->setAccessibleName(QStringLiteral("MIDI device"));
    m_device->setStyleSheet(comboStyle());
    m_device->setMinimumWidth(160);
    deviceRow->addWidget(m_device, 1);
    deviceRow->addWidget(new QLabel(QStringLiteral("Profile:"), group));
    m_profileCombo = new QComboBox(group);
    m_profileCombo->setObjectName(QStringLiteral("midiProfile"));
    m_profileCombo->setProperty("nereusSetupId", QStringLiteral("keys.midi.profile"));
    m_profileCombo->setAccessibleName(QStringLiteral("MIDI profile"));
    m_profileCombo->setStyleSheet(comboStyle());
    m_profileCombo->setMinimumWidth(120);
    deviceRow->addWidget(m_profileCombo);
    layout->addLayout(deviceRow);

    auto* const nameRow = new QHBoxLayout;
    nameRow->setSpacing(6);
    nameRow->addWidget(new QLabel(QStringLiteral("Profile name:"), group));
    m_name = new QLineEdit(group);
    m_name->setObjectName(QStringLiteral("midiProfileName"));
    m_name->setAccessibleName(QStringLiteral("Profile name"));
    m_name->setPlaceholderText(QStringLiteral("Type a name, then New or Rename"));
    m_name->setStyleSheet(lineEditStyle()
                          + QStringLiteral("QLineEdit:disabled { background: #141a22; color: #5a6a7a;"
                                           " border-color: #243240; }"));
    nameRow->addWidget(m_name, 1);
    m_new = new QPushButton(QStringLiteral("New"), group);
    m_new->setObjectName(QStringLiteral("midiProfileNew"));
    m_rename = new QPushButton(QStringLiteral("Rename"), group);
    m_rename->setObjectName(QStringLiteral("midiProfileRename"));
    m_delete = new QPushButton(QStringLiteral("Delete"), group);
    m_delete->setObjectName(QStringLiteral("midiProfileDelete"));
    for (QPushButton* const button : {m_new, m_rename, m_delete}) {
        button->setStyleSheet(smallButtonStyle());
        nameRow->addWidget(button);
    }
    layout->addLayout(nameRow);

    auto* const untested = new QLabel(
        QStringLiteral("Untested until someone tries a real device. A controller starts empty, "
                       "so learn each control you want to use."),
        group);
    untested->setObjectName(QStringLiteral("midiUntested"));
    untested->setWordWrap(true);
    untested->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    layout->addWidget(untested);

    auto* const stateRow = new QHBoxLayout;
    m_availability = new QLabel(group);
    m_availability->setObjectName(QStringLiteral("midiAvailability"));
    m_availability->setWordWrap(true);
    m_availability->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #c8d8e8; }"));
    stateRow->addWidget(m_availability, 1);
    m_last = new QLabel(group);
    m_last->setObjectName(QStringLiteral("midiLast"));
    m_last->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    stateRow->addWidget(m_last);
    layout->addLayout(stateRow);

    connect(m_device, &QComboBox::activated, this, [this](int index) {
        chooseDevice(m_device->itemData(index).toString());
    });
    connect(m_profileCombo, &QComboBox::activated, this, [this](int index) {
        switchProfile(m_profileCombo->itemText(index));
    });
    connect(m_name, &QLineEdit::textChanged, this, &MidiSetupPage::applyAvailability);
    connect(m_new, &QPushButton::clicked, this, &MidiSetupPage::createProfile);
    connect(m_rename, &QPushButton::clicked, this, &MidiSetupPage::renameProfile);
    connect(m_delete, &QPushButton::clicked, this, &MidiSetupPage::deleteProfile);
}

void MidiSetupPage::buildControls()
{
    m_controlsGroup = addSection(QStringLiteral("Controls"));
    m_controlsGroup->setObjectName(QStringLiteral("midiControlsGroup"));
    auto* const layout = qobject_cast<QVBoxLayout*>(m_controlsGroup->layout());

    auto* const learnRow = new QHBoxLayout;
    learnRow->setSpacing(6);
    learnRow->addWidget(new QLabel(QStringLiteral("Learn:"), m_controlsGroup));
    m_learn = new QComboBox(m_controlsGroup);
    m_learn->setObjectName(QStringLiteral("midiLearn"));
    m_learn->setProperty("nereusSetupId", QStringLiteral("keys.midi.learn"));
    m_learn->setAccessibleName(QStringLiteral("Action to learn"));
    m_learn->setStyleSheet(comboStyle());
    fillActionCombo(m_learn, QStringLiteral("None"));
    markUnavailableActions(m_learn);
    learnRow->addWidget(m_learn);
    m_learnText = new QLabel(learnPrompt(), m_controlsGroup);
    m_learnText->setObjectName(QStringLiteral("midiLearnText"));
    m_learnText->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    learnRow->addWidget(m_learnText);
    m_learnCancel = new QPushButton(QStringLiteral("Cancel"), m_controlsGroup);
    m_learnCancel->setObjectName(QStringLiteral("midiLearnCancel"));
    m_learnCancel->setStyleSheet(smallButtonStyle());
    learnRow->addWidget(m_learnCancel);
    learnRow->addStretch();
    layout->addLayout(learnRow);

    connect(m_learn, &QComboBox::activated, this, [this](int index) {
        const QString id = m_learn->itemData(index).toString();
        if (id.isEmpty()) {
            endLearn();
        } else {
            beginLearn(id);
        }
    });
    connect(m_learnCancel, &QPushButton::clicked, this, &MidiSetupPage::endLearn);

    m_table = new QTableWidget(1, kColumnCount, m_controlsGroup);
    m_table->setObjectName(QStringLiteral("midiList"));
    m_table->setProperty("nereusSetupId", QStringLiteral("keys.midi.controls"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("Control"), QStringLiteral("Kind"), QStringLiteral("Action"),
                                        QStringLiteral("Other direction"), QStringLiteral("Acts on"), QString()});
    m_table->setStyleSheet(listStyle());
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    QHeaderView* const header = m_table->horizontalHeader();
    header->setSectionResizeMode(kColumnControl, QHeaderView::Interactive);
    header->setSectionResizeMode(kColumnKind, QHeaderView::Fixed);
    header->setSectionResizeMode(kColumnAction, QHeaderView::Stretch);
    header->setSectionResizeMode(kColumnDown, QHeaderView::Stretch);
    header->setSectionResizeMode(kColumnActsOn, QHeaderView::Fixed);
    header->setSectionResizeMode(kColumnClear, QHeaderView::Fixed);
    m_table->setColumnWidth(kColumnControl, 96);
    m_table->setColumnWidth(kColumnKind, 116);
    m_table->setColumnWidth(kColumnActsOn, 116);
    m_table->setColumnWidth(kColumnClear, 56);
    m_table->setFixedHeight(kListHeight);

    auto* const empty = new QTableWidgetItem(QStringLiteral("No controls learned."));
    empty->setFlags(Qt::ItemIsEnabled);
    m_table->setItem(0, 0, empty);
    m_table->setSpan(0, 0, 1, kColumnCount);
    layout->addWidget(m_table);

    m_status = new QLabel(m_controlsGroup);
    m_status->setObjectName(QStringLiteral("midiStatus"));
    m_status->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
}

void MidiSetupPage::markUnavailableActions(QComboBox* combo) const
{
    auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
    if (model == nullptr) {
        return;
    }
    for (int i = 1; i < combo->count(); ++i) {
        const QString id = combo->itemData(i).toString();
        QStandardItem* const item = model->item(i);
        if (id.isEmpty() || item == nullptr) {
            continue;
        }
        const QString reason = m_reason ? m_reason(id) : QString();
        const Qt::ItemFlags usable = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        item->setFlags(reason.isEmpty() ? item->flags() | usable : item->flags() & ~usable);
        item->setToolTip(reason);
    }
}

// ---- Profiles ----

void MidiSetupPage::loadActiveProfile()
{
    const QStringList saved = MidiProfile::savedNames();
    QString name = MidiProfile::activeName();
    if (!saved.contains(name)) {
        name = saved.isEmpty() ? defaultProfileName() : saved.first();
    }
    m_profile = MidiProfile(name);
    m_profile.load();
    fillProfileCombo();
    syncRows();
}

void MidiSetupPage::fillProfileCombo()
{
    const QSignalBlocker blocker(m_profileCombo);
    m_profileCombo->clear();
    QStringList names = MidiProfile::savedNames();
    if (!names.contains(m_profile.name())) {
        names.append(m_profile.name());     // not saved until something is learned
        names.sort();
    }
    m_profileCombo->addItems(names);
    m_profileCombo->setCurrentText(m_profile.name());
}

void MidiSetupPage::persist()
{
    m_profile.save();
    MidiProfile::setActiveName(m_profile.name());
    if (m_host) {
        m_host->setMidiProfile(m_profile);
    }
}

QString MidiSetupPage::nameRefusal() const
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        return QStringLiteral("Type a name first.");
    }
    if (name == m_profile.name() || MidiProfile::savedNames().contains(name)) {
        return QStringLiteral("A profile named %1 already exists.").arg(name);
    }
    return {};
}

void MidiSetupPage::createProfile()
{
    if (!nameRefusal().isEmpty()) {
        return;
    }
    endLearn();
    // The one being left stays, with what it holds.
    m_profile.save();
    const QString name = m_name->text().trimmed();
    m_profile = MidiProfile(name);
    persist();
    m_name->clear();
    fillProfileCombo();
    syncRows();
    setStatus(QStringLiteral("Created profile %1. It has no controls.").arg(name));
}

void MidiSetupPage::renameProfile()
{
    if (!nameRefusal().isEmpty()) {
        return;
    }
    endLearn();
    const QString oldName = m_profile.name();
    const QString name = m_name->text().trimmed();
    MidiProfile renamed(name);
    const QList<MidiBinding> bindings = m_profile.bindings();
    for (const MidiBinding& binding : bindings) {
        renamed.bind(binding);
    }
    m_profile = renamed;
    persist();
    MidiProfile::removeSaved(oldName);
    m_name->clear();
    fillProfileCombo();
    applyAvailability();
    setStatus(QStringLiteral("Renamed profile %1 to %2.").arg(oldName, name));
}

void MidiSetupPage::deleteProfile()
{
    if (m_profileCombo->count() < 2) {
        return;
    }
    endLearn();
    const QString oldName = m_profile.name();
    MidiProfile::removeSaved(oldName);
    const QStringList saved = MidiProfile::savedNames();
    m_profile = MidiProfile(saved.isEmpty() ? defaultProfileName() : saved.first());
    m_profile.load();
    persist();
    fillProfileCombo();
    syncRows();
    applyAvailability();
    setStatus(QStringLiteral("Deleted profile %1.").arg(oldName));
}

void MidiSetupPage::switchProfile(const QString& name)
{
    if (name == m_profile.name()) {
        return;
    }
    endLearn();
    m_profile.save();   // a profile still unsaved stays in the list
    m_profile = MidiProfile(name);
    m_profile.load();
    persist();
    fillProfileCombo();
    syncRows();
    setStatus({});
}

// ---- The list ----

void MidiSetupPage::addRow(const MidiBinding& binding)
{
    const QString key = binding.controlKey;
    Row row;
    row.controlKey = key;
    const int tableRow = int(m_rows.size()) + 1;
    m_table->insertRow(tableRow);

    auto* const control = new QTableWidgetItem(controlName(key));
    control->setFlags(Qt::ItemIsEnabled);
    control->setToolTip(key);
    m_table->setItem(tableRow, kColumnControl, control);

    row.kind = new QComboBox;
    row.kind->setObjectName(QStringLiteral("midiKind:") + key);
    row.kind->setAccessibleName(QStringLiteral("Kind"));
    row.kind->setStyleSheet(comboStyle());
    row.kind->installEventFilter(this);
    for (const MidiControlKind kind :
         {MidiControlKind::Button, MidiControlKind::AbsoluteKnob, MidiControlKind::RelativeKnob}) {
        row.kind->addItem(kindName(kind), int(kind));
        const int index = row.kind->count() - 1;
        const QString refusal = kindRefusal(key, kind);
        row.kind->setItemData(index, refusal.isEmpty() ? kindHelp(kind) : refusal, Qt::ToolTipRole);
        if (!refusal.isEmpty()) {
            disableComboItem(row.kind, index, refusal);
        }
    }
    row.cells[0] = makeCell(row.kind);
    m_table->setCellWidget(tableRow, kColumnKind, row.cells[0]);
    connect(row.kind, &QComboBox::activated, this, [this, key](int index) {
        Row* const found = rowFor(key);
        onKindChosen(key, MidiControlKind(found->kind->itemData(index).toInt()));
    });

    row.action = new QComboBox;
    row.action->setObjectName(QStringLiteral("midiAction:") + key);
    row.action->setAccessibleName(QStringLiteral("Action"));
    row.action->setStyleSheet(comboStyle());
    row.action->installEventFilter(this);
    fillActionCombo(row.action, QStringLiteral("None"));
    markUnavailableActions(row.action);
    row.cells[1] = makeCell(row.action);
    m_table->setCellWidget(tableRow, kColumnAction, row.cells[1]);
    connect(row.action, &QComboBox::activated, this, [this, key](int index) {
        onActionChosen(key, rowFor(key)->action->itemData(index).toString());
    });

    row.down = new QComboBox;
    row.down->setObjectName(QStringLiteral("midiDown:") + key);
    row.down->setAccessibleName(QStringLiteral("Other direction"));
    row.down->setStyleSheet(comboStyle());
    row.down->installEventFilter(this);
    fillActionCombo(row.down, QStringLiteral("None"));
    markUnavailableActions(row.down);
    row.cells[2] = makeCell(row.down);
    m_table->setCellWidget(tableRow, kColumnDown, row.cells[2]);
    connect(row.down, &QComboBox::activated, this, [this, key](int index) {
        onDownChosen(key, rowFor(key)->down->itemData(index).toString());
    });

    row.actsOn = new QComboBox;
    row.actsOn->setObjectName(QStringLiteral("midiActsOn:") + key);
    row.actsOn->setAccessibleName(QStringLiteral("Acts on"));
    row.actsOn->setStyleSheet(comboStyle());
    row.actsOn->installEventFilter(this);
    row.cells[3] = makeCell(row.actsOn);
    m_table->setCellWidget(tableRow, kColumnActsOn, row.cells[3]);
    connect(row.actsOn, &QComboBox::activated, this, [this, key](int index) {
        onActsOnChosen(key, rowFor(key)->actsOn->itemData(index).toString());
    });

    row.clear = new QPushButton(QStringLiteral("Clear"));
    row.clear->setObjectName(QStringLiteral("midiClear:") + key);
    row.clear->setAccessibleName(QStringLiteral("Clear %1").arg(controlName(key)));
    row.clear->setStyleSheet(smallButtonStyle());
    m_table->setCellWidget(tableRow, kColumnClear, makeCell(row.clear));
    connect(row.clear, &QPushButton::clicked, this, [this, key] {
        endLearn();
        m_profile.clearControl(key);
        persist();
        syncRows();
        setStatus(QStringLiteral("Cleared %1.").arg(controlName(key)));
    });

    m_rows.push_back(row);
}

void MidiSetupPage::retireRow(int index)
{
    const int tableRow = index + 1;
    for (int column = 0; column < kColumnCount; ++column) {
        retireNames(m_table->cellWidget(tableRow, column));
    }
    m_table->removeRow(tableRow);
    m_rows.erase(m_rows.begin() + index);
}

// The rows follow the profile. A row that stays keeps its widgets, because one of
// them may be what called here.
void MidiSetupPage::syncRows()
{
    const QList<MidiBinding>& bindings = m_profile.bindings();
    int common = 0;
    while (common < int(m_rows.size()) && common < bindings.size()
           && m_rows[std::size_t(common)].controlKey == bindings.at(common).controlKey) {
        ++common;
    }
    // A row cleared from the middle leaves the ones after it to line up again.
    for (int i = int(m_rows.size()) - 1; i >= common; --i) {
        const QString key = m_rows[std::size_t(i)].controlKey;
        const bool stays = std::any_of(bindings.cbegin(), bindings.cend(), [&key](const MidiBinding& binding) {
            return binding.controlKey == key;
        });
        if (!stays) {
            retireRow(i);
        }
    }
    bool sameOrder = int(m_rows.size()) <= bindings.size();
    for (int i = 0; sameOrder && i < int(m_rows.size()); ++i) {
        sameOrder = m_rows[std::size_t(i)].controlKey == bindings.at(i).controlKey;
    }
    if (!sameOrder) {
        for (int i = int(m_rows.size()) - 1; i >= 0; --i) {
            retireRow(i);
        }
    }
    for (int i = int(m_rows.size()); i < bindings.size(); ++i) {
        addRow(bindings.at(i));
    }
    for (int i = 0; i < bindings.size(); ++i) {
        syncRow(m_rows[std::size_t(i)], bindings.at(i));
    }
    m_table->setRowHidden(0, !m_rows.empty());
    applyAvailability();
}

void MidiSetupPage::syncRow(Row& row, const MidiBinding& binding)
{
    setComboData(row.kind, int(binding.kind));
    setComboData(row.action, binding.actionId);
    setComboData(row.down, binding.actionIdDown);

    const bool isButton = binding.kind == MidiControlKind::Button;
    row.down->setEnabled(!isButton);
    row.action->setToolTip(row.action->currentText());
    setReason(row.down, isButton ? QStringLiteral("A button has no other direction.") : row.down->currentText());

    const HotKeyAction* const action = HotKeyActionList::find(binding.actionId);
    const HotKeyScope scope = action != nullptr ? action->scope : HotKeyScope::Slice;
    const QSignalBlocker blocker(row.actsOn);
    row.actsOn->clear();
    if (scope == HotKeyScope::Slice) {
        row.actsOn->addItem(QStringLiteral("Selected slice"), QString());
        for (const QChar letter : {QLatin1Char('A'), QLatin1Char('B'), QLatin1Char('C'), QLatin1Char('D')}) {
            row.actsOn->addItem(QStringLiteral("Slice %1").arg(letter), QString(letter));
        }
        row.actsOn->setCurrentIndex(
            binding.pin.isNull() ? 0 : std::max(0, row.actsOn->findData(QString(binding.pin))));
        row.actsOn->setEnabled(true);
        setReason(row.actsOn, QString());
    } else {
        row.actsOn->addItem(fixedTargetName(scope));
        row.actsOn->setEnabled(false);
        setReason(row.actsOn, fixedTargetReason(scope));
    }
}

MidiSetupPage::Row* MidiSetupPage::rowFor(const QString& controlKey)
{
    for (Row& row : m_rows) {
        if (row.controlKey == controlKey) {
            return &row;
        }
    }
    return nullptr;
}

const MidiBinding* MidiSetupPage::bindingFor(const QString& controlKey) const
{
    const QList<MidiBinding>& bindings = m_profile.bindings();
    for (const MidiBinding& binding : bindings) {
        if (binding.controlKey == controlKey) {
            return &binding;
        }
    }
    return nullptr;
}

// A refusal leaves the profile as it was and puts the row back.
void MidiSetupPage::commit(const MidiBinding& binding, const QString& doneText)
{
    const QString refusal = m_profile.bind(binding);
    if (refusal.isEmpty()) {
        persist();
        setStatus(doneText);
    } else {
        setStatus(refusal);
    }
    syncRows();
}

void MidiSetupPage::onKindChosen(const QString& controlKey, MidiControlKind kind)
{
    const MidiBinding* const current = bindingFor(controlKey);
    if (current == nullptr) {
        return;
    }
    MidiBinding binding = *current;
    binding.kind = kind;
    if (kind == MidiControlKind::Button) {
        binding.actionIdDown.clear();
    } else if (binding.actionIdDown.isEmpty()) {
        binding.actionIdDown = MidiProfile::otherDirection(binding.actionId);
    }
    commit(binding, QStringLiteral("%1 is now: %2.").arg(controlName(controlKey), kindName(kind)));
}

void MidiSetupPage::onActionChosen(const QString& controlKey, const QString& id)
{
    const MidiBinding* const current = bindingFor(controlKey);
    if (current == nullptr) {
        return;
    }
    MidiBinding binding = *current;
    // The other direction follows only if it was this action's own.
    if (binding.kind != MidiControlKind::Button
        && binding.actionIdDown == MidiProfile::otherDirection(binding.actionId)) {
        binding.actionIdDown = MidiProfile::otherDirection(id);
    }
    binding.actionId = id;
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action != nullptr && action->scope != HotKeyScope::Slice) {
        binding.pin = QChar();
    }
    commit(binding, QStringLiteral("%1 changed.").arg(controlName(controlKey)));
}

void MidiSetupPage::onDownChosen(const QString& controlKey, const QString& id)
{
    const MidiBinding* const current = bindingFor(controlKey);
    if (current == nullptr) {
        return;
    }
    MidiBinding binding = *current;
    binding.actionIdDown = id;
    commit(binding, QStringLiteral("%1 changed.").arg(controlName(controlKey)));
}

void MidiSetupPage::onActsOnChosen(const QString& controlKey, const QString& letter)
{
    const MidiBinding* const current = bindingFor(controlKey);
    if (current == nullptr) {
        return;
    }
    MidiBinding binding = *current;
    binding.pin = letter.isEmpty() ? QChar() : letter.at(0);
    commit(binding, QStringLiteral("%1 changed.").arg(controlName(controlKey)));
}

// ---- The device ----

void MidiSetupPage::setSupportedForTest(bool supported)
{
    m_supported = supported;
    refreshDevices();
}

void MidiSetupPage::refreshDevices()
{
    const QStringList ports = m_supported ? MidiInput::portNames() : QStringList{};
    const QString saved = m_host ? ControllerHost::savedMidiDevice() : QString();
    m_ports = ports;

    QStringList names{QString()};
    QStringList labels{QStringLiteral("None")};
    QString current;
    for (const QString& port : ports) {
        names.append(port);
        labels.append(port);
        if (current.isEmpty() && !saved.isEmpty() && midiPortNameMatches(port, saved)) {
            current = port;
        }
    }
    if (!saved.isEmpty() && current.isEmpty()) {
        names.append(saved);
        labels.append(QStringLiteral("%1 (not plugged in)").arg(saved));
        current = saved;
    }

    // Left alone when nothing changed: this runs on a timer, and rebuilding would
    // close the list under someone choosing from it.
    QStringList shown;
    for (int i = 0; i < m_device->count(); ++i) {
        shown.append(m_device->itemText(i));
    }
    const QSignalBlocker blocker(m_device);
    if (shown != labels) {
        m_device->clear();
        for (int i = 0; i < names.size(); ++i) {
            m_device->addItem(labels.at(i), names.at(i));
        }
    }
    m_device->setCurrentIndex(std::max(0, m_device->findData(current)));
    applyAvailability();
}

void MidiSetupPage::chooseDevice(const QString& name)
{
    endLearn();
    m_connectionReason.clear();
    if (m_host) {
        m_host->setMidiDevice(name);
    }
    refreshDevices();
}

QString MidiSetupPage::unavailableReason() const
{
    if (!m_host || !m_bindings) {
        return noHostReason();
    }
    if (!m_supported) {
        return noSupportReason();
    }
    if (m_ports.isEmpty()) {
        return noDeviceReason();
    }
    const QString saved = ControllerHost::savedMidiDevice();
    if (saved.isEmpty()) {
        return chooseDeviceReason();
    }
    const bool pluggedIn = std::any_of(m_ports.cbegin(), m_ports.cend(), [&saved](const QString& port) {
        return midiPortNameMatches(port, saved);
    });
    if (!pluggedIn) {
        return deviceGoneReason();
    }
    if (!m_bindings->enabled()) {
        return switchedOffReason();
    }
    return {};
}

void MidiSetupPage::applyAvailability()
{
    const QString reason = unavailableReason();
    const bool usable = reason.isEmpty();
    if (!usable) {
        endLearn();
    }

    // The device stays open to change whenever there is a choice to make, or
    // a saved device that is gone would be stuck.
    const bool canChoose = m_host && m_bindings && m_supported && m_device->count() > 1;
    m_device->setEnabled(canChoose);
    setReason(m_device, canChoose ? QString() : reason);

    m_profileCombo->setEnabled(usable);
    setReason(m_profileCombo, reason);
    m_name->setEnabled(usable);
    setReason(m_name, reason);

    const QString nameBlock = usable ? nameRefusal() : reason;
    for (QPushButton* const button : {m_new, m_rename}) {
        button->setEnabled(nameBlock.isEmpty());
        setReason(button, nameBlock);
    }
    const QString deleteBlock = !usable ? reason
        : m_profileCombo->count() < 2   ? QStringLiteral("The only profile can't be deleted. Clear its controls instead.")
                                        : QString();
    m_delete->setEnabled(deleteBlock.isEmpty());
    setReason(m_delete, deleteBlock);

    m_controlsGroup->setEnabled(usable);
    setReason(m_controlsGroup, reason);

    const QString learnBlock = !usable ? reason : m_learnId.isEmpty() ? QStringLiteral("Nothing is being learned.") : QString();
    m_learnCancel->setEnabled(learnBlock.isEmpty());
    setReason(m_learnCancel, learnBlock);

    m_availability->setText(usable ? m_connectionReason : reason);
}

// ---- Learn ----

void MidiSetupPage::beginLearn(const QString& id)
{
    if (!m_host || !unavailableReason().isEmpty()) {
        return;
    }
    const bool wasLearning = !m_learnId.isEmpty();
    m_learnId = id;
    setStatus({});
    m_learnText->setText(QStringLiteral("Now move a control. Esc cancels."));
    if (!wasLearning) {
        // The last filter installed runs first, so Esc ends learn before it can
        // close Setup.
        qApp->installEventFilter(this);
    }
    MidiInput* const midi = m_host->midi();
    QMetaObject::invokeMethod(midi, [midi] { midi->beginLearn(); }, Qt::QueuedConnection);
    applyAvailability();
}

void MidiSetupPage::endLearn()
{
    if (m_learnId.isEmpty()) {
        return;
    }
    m_learnId.clear();
    qApp->removeEventFilter(this);
    if (m_host) {
        MidiInput* const midi = m_host->midi();
        QMetaObject::invokeMethod(midi, [midi] { midi->cancelLearn(); }, Qt::QueuedConnection);
    }
    const QSignalBlocker blocker(m_learn);
    m_learn->setCurrentIndex(0);
    m_learnText->setText(learnPrompt());
    m_learnCancel->setEnabled(false);
    setReason(m_learnCancel, QStringLiteral("Nothing is being learned."));
}

void MidiSetupPage::onLearned(const QString& controlKey, MidiControlKind kind)
{
    if (m_learnId.isEmpty()) {
        return;     // canceled while this was on its way
    }
    const QString id = m_learnId;
    endLearn();

    // A kind the operator corrected stays through learning the control again.
    const MidiBinding* const current = bindingFor(controlKey);
    MidiBinding binding{};
    binding.controlKey = controlKey;
    binding.kind = current != nullptr ? current->kind : kind;
    binding.actionId = id;
    if (binding.kind != MidiControlKind::Button) {
        binding.actionIdDown = MidiProfile::otherDirection(id);
    }
    const HotKeyAction* const action = HotKeyActionList::find(id);
    const QString actionName = action != nullptr ? action->name : id;
    const QString refusal = m_profile.bind(binding);
    if (!refusal.isEmpty()) {
        setStatus(refusal);
        return;
    }
    persist();
    syncRows();
    const QString text = QStringLiteral("%1 learned as %2.").arg(controlName(controlKey), actionName);
    m_last->setText(text);
    setStatus(text);
    if (Row* const row = rowFor(controlKey)) {
        m_table->scrollToItem(m_table->item(int(row - m_rows.data()) + 1, kColumnControl));
    }
}

void MidiSetupPage::setStatus(const QString& text)
{
    m_status->setText(text);
}

bool MidiSetupPage::eventFilter(QObject* watched, QEvent* event)
{
    // Scrolling the list with the pointer over a row must not change that row.
    if (event->type() == QEvent::Wheel && qobject_cast<QComboBox*>(watched) != nullptr) {
        event->ignore();
        return true;
    }
    if (m_learnId.isEmpty()
        || (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress
            && event->type() != QEvent::KeyRelease)
        || static_cast<QKeyEvent*>(event)->key() != Qt::Key_Escape) {
        return SetupPage::eventFilter(watched, event);
    }
    QWidget* const ours = window();
    const bool inOurWindow = watched->isWidgetType() ? static_cast<QWidget*>(watched)->window() == ours
                                                     : watched == static_cast<QObject*>(ours->windowHandle());
    if (!inOurWindow) {
        return SetupPage::eventFilter(watched, event);
    }
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();    // claim it, so the press reaches this filter
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        endLearn();
        setStatus(QStringLiteral("Learn canceled."));
    }
    return true;
}

void MidiSetupPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    refreshDevices();
    m_deviceTimer->start();
}

void MidiSetupPage::hideEvent(QHideEvent* event)
{
    m_deviceTimer->stop();
    endLearn();
    SetupPage::hideEvent(event);
}

} // namespace NereusSDR
