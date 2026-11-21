// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 11: what the MIDI page
//                shares moved to HotKeySetupCommon.h.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/setup/HotKeySetupPages.h"

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/StyleConstants.h"
#include "gui/hotkeys/KeyboardRouter.h"
#include "gui/setup/HotKeySetupCommon.h"
#include "gui/setup/KeyboardMapWidget.h"
#include "gui/styles/WorkspacePreview.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWindow>

#include <utility>

namespace NereusSDR {

using namespace HotKeySetup;

namespace {

constexpr int kColumnAction = 0;
constexpr int kColumnKey = 1;
constexpr int kColumnActsOn = 2;
constexpr int kColumnButtons = 3;
constexpr int kListHeight = 320;
constexpr int kRowHeight = 30;
constexpr int kChipsPerRow = 5;
constexpr int kChipSwatchLeft = 6;
constexpr qint64 kMostImportBytes = 1024 * 1024;  // far past any real shortcuts file

QColor changedKeyColor() { return WorkspacePreview::roleColour(QColor(0x00, 0xb4, 0xd8), "accent"); }
QColor noKeyColor() { return WorkspacePreview::roleColour(QColor(0x80, 0x90, 0xa0), "text-secondary"); }
QColor captureColor() { return WorkspacePreview::roleColour(QColor(0x10, 0x22, 0x3a), "sel-bg"); }
QColor keyTextColor() { return WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "text"); }
QColor offKeyTextColor() { return WorkspacePreview::roleColour(QColor(0x5a, 0x6a, 0x7a), "text-inactive"); }

QString noBindingsReason()
{
    return QStringLiteral("Shortcuts belong to a radio window, and this page has none.");
}

// The map's order for its legend, as the mockup has it.
QStringList legendGroups()
{
    return {QStringLiteral("Tuning"), QStringLiteral("Filter"), QStringLiteral("Audio"),
            QStringLiteral("Band and mode"), QStringLiteral("Display"), QStringLiteral("DSP"),
            QStringLiteral("Transmit"), QStringLiteral("CW"), QStringLiteral("Voice"),
            KeyboardMapWidget::menuGroup()};
}

QString chipStyle()
{
    return QStringLiteral(
        "QPushButton { background: #111120; border: 1px solid #304050; border-radius: 9px;"
        " color: #c8d8e8; font-size: 11px; padding: 1px 8px 1px 21px; }"
        "QPushButton:hover { background: #1a2a3a; }"
        "QPushButton:checked { border-color: #ffffff; }"
        "QPushButton:disabled { color: #5a6a7a; }");
}

} // namespace

KeyboardPage::KeyboardPage(HotKeyBindings* bindings, QWidget* parent)
    : SetupPage(QStringLiteral("Keyboard"), parent)
    , m_bindings(bindings)
{
    buildSwitch();
    buildMap();
    buildActions();

    if (m_bindings) {
        connect(m_bindings.data(), &HotKeyBindings::bindingsChanged, this, &KeyboardPage::refreshAll);
        connect(m_bindings.data(), &HotKeyBindings::enabledChanged, this, &KeyboardPage::applyEnabled);
    }
    refreshAll();
    applyEnabled(m_bindings ? m_bindings->enabled() : false);
}

void KeyboardPage::setUnavailableReasonProvider(ReasonProvider provider)
{
    m_reason = std::move(provider);
    refreshAvailability();
}

void KeyboardPage::refreshAvailability()
{
    refreshAll();
}

QString KeyboardPage::reasonFor(const QString& id) const
{
    if (!m_bindings) {
        return noBindingsReason();
    }
    return m_reason ? m_reason(id) : QString();
}

void KeyboardPage::buildSwitch()
{
    auto* const row = new QHBoxLayout;
    m_useShortcuts = new QCheckBox(QStringLiteral("Use shortcuts"));
    m_useShortcuts->setObjectName(QStringLiteral("hotKeyUseShortcuts"));
    m_useShortcuts->setProperty("nereusSetupId", QStringLiteral("keys.keyboard.useShortcuts"));
    setReason(m_useShortcuts, QStringLiteral("Turn off the keyboard, MIDI, knob and deck shortcuts together."));
    row->addWidget(m_useShortcuts);
    row->addStretch();
    addContent(row);

    if (m_bindings) {
        m_useShortcuts->setChecked(m_bindings->enabled());
        connect(m_useShortcuts, &QCheckBox::toggled, this, [this](bool on) {
            if (m_bindings) {
                m_bindings->setEnabled(on);
            }
        });
    } else {
        m_useShortcuts->setEnabled(false);
        setReason(m_useShortcuts, noBindingsReason());
    }
}

void KeyboardPage::buildMap()
{
    m_mapGroup = addSection(QStringLiteral("Keyboard map"));
    m_mapGroup->setObjectName(QStringLiteral("hotKeyMapGroup"));
    auto* const layout = qobject_cast<QVBoxLayout*>(m_mapGroup->layout());

    auto* const layerRow = new QHBoxLayout;
    layerRow->setSpacing(3);
    m_layers = new QButtonGroup(this);
    const QString command = KeyboardMapWidget::commandKeyName();
    const QStringList layerNames{QStringLiteral("Plain"), QStringLiteral("Shift"), command,
                                 command + QStringLiteral("+Shift")};
    for (int i = 0; i < layerNames.size(); ++i) {
        auto* const button = new QPushButton(layerNames.at(i), m_mapGroup);
        button->setObjectName(QStringLiteral("hotKeyLayer%1").arg(i));
        button->setCheckable(true);
        button->setStyleSheet(smallButtonStyle());
        button->setChecked(i == 0);
        m_layers->addButton(button, i);
        layerRow->addWidget(button);
    }
    layerRow->addStretch();
    layout->addLayout(layerRow);

    // Two rows, so the legend fits Setup's width without a sideways scroll.
    auto* const chipRows = new QVBoxLayout;
    chipRows->setSpacing(3);
    auto* chipRow = new QHBoxLayout;
    chipRow->setSpacing(4);
    chipRows->addLayout(chipRow);
    int chipsOnRow = 0;
    for (const QString& group : legendGroups()) {
        if (chipsOnRow == kChipsPerRow) {
            chipRow->addStretch();
            chipRow = new QHBoxLayout;
            chipRow->setSpacing(4);
            chipRows->addLayout(chipRow);
            chipsOnRow = 0;
        }
        ++chipsOnRow;
        auto* const chip = new QPushButton(group, m_mapGroup);
        chip->setObjectName(QStringLiteral("hotKeyChip:") + group);
        chip->setCheckable(true);
        auto* const chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(kChipSwatchLeft, 0, 0, 0);
        chipLayout->addWidget(new KeyGroupSwatch(KeyboardMapWidget::groupColor(group), chip), 0, Qt::AlignVCenter);
        chipLayout->addStretch();
        chip->setStyleSheet(chipStyle());
        setReason(chip, QStringLiteral("Show only the %1 keys brightly.").arg(group));
        chipRow->addWidget(chip);
        m_chips.push_back(chip);
        connect(chip, &QPushButton::toggled, this, [this, chip, group](bool on) {
            for (QPushButton* const other : m_chips) {
                if (other != chip && on) {
                    const QSignalBlocker blocker(other);
                    other->setChecked(false);
                }
            }
            m_map->setHighlightedGroup(on ? group : QString());
        });
    }
    chipRow->addStretch();
    layout->addLayout(chipRows);

    m_map = new KeyboardMapWidget(m_bindings.data(), m_mapGroup);
    m_map->setObjectName(QStringLiteral("hotKeyMap"));
    layout->addWidget(m_map);

    connect(m_layers, &QButtonGroup::idClicked, this, [this](int layer) {
        m_pick.clear();
        m_map->setPickedChord({});
        m_map->setLayer(layer);
        refreshPick();
    });
    connect(m_map, &KeyboardMapWidget::keyClicked, this, &KeyboardPage::onKeyClicked);

    auto* const pickRow = new QHBoxLayout;
    pickRow->setSpacing(6);
    m_pickChord = new QLabel(m_mapGroup);
    m_pickChord->setObjectName(QStringLiteral("hotKeyPickChord"));
    m_pickChord->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #c8d8e8; }"));
    m_pickText = new QLabel(m_mapGroup);
    m_pickText->setObjectName(QStringLiteral("hotKeyPickText"));
    m_pickNote = new QLabel(m_mapGroup);
    m_pickNote->setObjectName(QStringLiteral("hotKeyPickNote"));
    m_pickNote->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_pickChange = new QPushButton(QStringLiteral("Change"), m_mapGroup);
    m_pickChange->setObjectName(QStringLiteral("hotKeyPickChange"));
    m_pickChange->setStyleSheet(smallButtonStyle());
    m_pickClear = new QPushButton(QStringLiteral("Clear"), m_mapGroup);
    m_pickClear->setObjectName(QStringLiteral("hotKeyPickClear"));
    m_pickClear->setStyleSheet(smallButtonStyle());
    m_pickAssign = new QComboBox(m_mapGroup);
    m_pickAssign->setObjectName(QStringLiteral("hotKeyPickAssign"));
    m_pickAssign->setAccessibleName(QStringLiteral("Action for this key"));
    m_pickAssign->setStyleSheet(comboStyle());
    m_pickAssign->addItem(QStringLiteral("Choose an action"));
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        m_pickAssign->addItem(action.name, action.id);
    }
    pickRow->addWidget(m_pickChord);
    pickRow->addWidget(m_pickText);
    pickRow->addWidget(m_pickNote);
    pickRow->addWidget(m_pickChange);
    pickRow->addWidget(m_pickClear);
    pickRow->addWidget(m_pickAssign);
    pickRow->addStretch();
    layout->addLayout(pickRow);

    connect(m_pickChange, &QPushButton::clicked, this, [this] {
        const QString owner = m_bindings ? m_bindings->actionForChord(m_pick) : QString();
        if (owner.isEmpty()) {
            return;
        }
        {
            const QSignalBlocker searchBlocker(m_search);
            const QSignalBlocker groupBlocker(m_groupFilter);
            m_search->clear();
            m_groupFilter->setCurrentIndex(0);
        }
        applyFilter();
        beginCapture(owner);
    });
    connect(m_pickClear, &QPushButton::clicked, this, [this] {
        const QString owner = m_bindings ? m_bindings->actionForChord(m_pick) : QString();
        if (!owner.isEmpty()) {
            m_bindings->clearKey(owner, m_pick);
        }
    });
    connect(m_pickAssign, &QComboBox::activated, this, [this](int index) {
        const QString id = m_pickAssign->itemData(index).toString();
        m_pickAssign->setCurrentIndex(0);
        if (!id.isEmpty()) {
            tryBind(id, m_pick);
        }
    });
}

void KeyboardPage::buildActions()
{
    m_actionsGroup = addSection(QStringLiteral("Actions"));
    m_actionsGroup->setObjectName(QStringLiteral("hotKeyActionsGroup"));
    auto* const layout = qobject_cast<QVBoxLayout*>(m_actionsGroup->layout());

    auto* const filterRow = new QHBoxLayout;
    m_search = new QLineEdit(m_actionsGroup);
    m_search->setObjectName(QStringLiteral("hotKeySearch"));
    m_search->setPlaceholderText(QStringLiteral("Search actions or keys"));
    m_search->setStyleSheet(lineEditStyle());
    m_groupFilter = new QComboBox(m_actionsGroup);
    m_groupFilter->setObjectName(QStringLiteral("hotKeyGroupFilter"));
    m_groupFilter->setAccessibleName(QStringLiteral("Category"));
    m_groupFilter->setStyleSheet(comboStyle());
    m_groupFilter->addItem(QStringLiteral("All categories"));
    for (const QString& group : groupsInOrder()) {
        m_groupFilter->addItem(group, group);
    }
    filterRow->addWidget(m_search, 1);
    filterRow->addWidget(m_groupFilter);
    layout->addLayout(filterRow);
    connect(m_search, &QLineEdit::textChanged, this, &KeyboardPage::applyFilter);
    connect(m_groupFilter, &QComboBox::currentIndexChanged, this, &KeyboardPage::applyFilter);

    const QList<HotKeyAction>& actions = HotKeyActionList::all();
    m_table = new QTableWidget(int(actions.size()) + 1, 4, m_actionsGroup);
    m_table->setObjectName(QStringLiteral("hotKeyList"));
    m_table->setProperty("nereusSetupId", QStringLiteral("keys.keyboard.actions"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("Action"), QStringLiteral("Key"),
                                        QStringLiteral("Acts on"), QString()});
    m_table->setStyleSheet(
        QStringLiteral("QTableWidget { background: #131326; color: #c8d8e8;"
                       "  gridline-color: #304050; border: 1px solid #304050; }"
                       "QTableWidget::item { padding: 2px 4px; border-bottom: 1px solid #1a2030; }"
                       "QTableWidget::item:selected { background: #1a3050; color: #c8d8e8; }"
                       "QHeaderView::section { background: #1a2030; color: #8aa8c0;"
                       "  border: 1px solid #304050; padding: 4px; }"));
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnAction, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnKey, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnActsOn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnButtons, QHeaderView::Fixed);
    m_table->setColumnWidth(kColumnKey, 112);
    m_table->setColumnWidth(kColumnActsOn, 124);
    m_table->setColumnWidth(kColumnButtons, 170);
    m_table->setFixedHeight(kListHeight);
    layout->addWidget(m_table);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this, &KeyboardPage::fitMessageRow);

    m_rows.resize(std::size_t(actions.size()));
    for (int i = 0; i < actions.size(); ++i) {
        Row& row = m_rows[std::size_t(i)];
        row.index = i;
        buildRow(row, actions.at(i));
    }

    m_noMatchRow = int(actions.size());
    auto* const noMatch = new QTableWidgetItem(QStringLiteral("No action matches."));
    noMatch->setFlags(Qt::ItemIsEnabled);
    noMatch->setForeground(noKeyColor());
    m_table->setItem(m_noMatchRow, 0, noMatch);
    m_table->setSpan(m_noMatchRow, 0, 1, 4);
    m_table->setRowHidden(m_noMatchRow, true);

    auto* const buttons = new QHBoxLayout;
    auto* const resetAll = new QPushButton(QStringLiteral("Reset all to defaults"), m_actionsGroup);
    resetAll->setObjectName(QStringLiteral("hotKeyResetAll"));
    resetAll->setStyleSheet(smallButtonStyle());
    auto* const importButton = new QPushButton(QStringLiteral("Import"), m_actionsGroup);
    importButton->setObjectName(QStringLiteral("hotKeyImport"));
    importButton->setStyleSheet(smallButtonStyle());
    auto* const exportButton = new QPushButton(QStringLiteral("Export"), m_actionsGroup);
    exportButton->setObjectName(QStringLiteral("hotKeyExport"));
    exportButton->setStyleSheet(smallButtonStyle());
    buttons->addWidget(resetAll);
    buttons->addStretch();
    buttons->addWidget(importButton);
    buttons->addWidget(exportButton);
    layout->addLayout(buttons);

    m_status = new QLabel(m_actionsGroup);
    m_status->setObjectName(QStringLiteral("hotKeyStatus"));
    m_status->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_skipped = new QPlainTextEdit(m_actionsGroup);
    m_skipped->setObjectName(QStringLiteral("hotKeySkippedLines"));
    m_skipped->setReadOnly(true);
    m_skipped->setFixedHeight(84);
    m_skipped->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: #131326; color: #c8d8e8; border: 1px solid #304050;"
                       " font-size: 11px; }"));
    m_skipped->setVisible(false);
    layout->addWidget(m_skipped);

    if (m_bindings) {
        connect(resetAll, &QPushButton::clicked, this, [this] {
            endCapture();
            dismissMessage();
            m_bindings->resetAll();
            setStatus(QStringLiteral("All keys back to defaults."));
        });
        connect(importButton, &QPushButton::clicked, this, &KeyboardPage::onImportClicked);
        connect(exportButton, &QPushButton::clicked, this, &KeyboardPage::onExportClicked);
    } else {
        for (QPushButton* const button : {resetAll, importButton, exportButton}) {
            button->setEnabled(false);
            setReason(button, noBindingsReason());
        }
    }
}

void KeyboardPage::buildRow(Row& row, const HotKeyAction& action)
{
    row.id = action.id;
    const QString id = action.id;

    row.nameCell = new QWidget;
    makeCellClear(row.nameCell);
    auto* const nameLayout = new QVBoxLayout(row.nameCell);
    nameLayout->setContentsMargins(4, 4, 4, 2);
    nameLayout->setSpacing(3);

    QString shownName = action.name;
    if (action.gesture == HotKeyGesture::Hold) {
        shownName += QStringLiteral(" <span style=\"color:%1\">hold</span>").arg(WorkspacePreview::roleColourName("#8090a0", "text-secondary"));
    }
    row.name = new QLabel(shownName, row.nameCell);
    row.name->setObjectName(QStringLiteral("hotKeyName:") + id);
    row.name->setStyleSheet(QStringLiteral("QLabel { background: transparent; color: #c8d8e8; }"
                                           "QLabel:disabled { color: #5a6a7a; }"));
    nameLayout->addWidget(row.name);

    row.messageFrame = new QFrame(row.nameCell);
    row.messageFrame->setObjectName(QStringLiteral("hotKeyMessageFrame"));
    row.messageFrame->setStyleSheet(
        QStringLiteral("QFrame#hotKeyMessageFrame { background: #2a1a1a; border: 1px solid #6e2c2c;"
                       " border-radius: 4px; }"));
    auto* const messageLayout = new QVBoxLayout(row.messageFrame);
    messageLayout->setContentsMargins(6, 4, 6, 4);
    messageLayout->setSpacing(4);
    row.message = new QLabel(row.messageFrame);
    row.message->setObjectName(QStringLiteral("hotKeyMessage:") + id);
    row.message->setWordWrap(true);
    row.message->setStyleSheet(QStringLiteral("QLabel { background: transparent; color: #e8d8d8; }"));
    messageLayout->addWidget(row.message);
    auto* const messageButtons = new QHBoxLayout;
    row.replace = new QPushButton(QStringLiteral("Replace"), row.messageFrame);
    row.replace->setObjectName(QStringLiteral("hotKeyReplace:") + id);
    row.replace->setStyleSheet(smallButtonStyle());
    row.dismiss = new QPushButton(QStringLiteral("OK"), row.messageFrame);
    row.dismiss->setObjectName(QStringLiteral("hotKeyDismiss:") + id);
    row.dismiss->setStyleSheet(smallButtonStyle());
    messageButtons->addWidget(row.replace);
    messageButtons->addWidget(row.dismiss);
    messageButtons->addStretch();
    messageLayout->addLayout(messageButtons);
    row.messageFrame->setVisible(false);
    nameLayout->addWidget(row.messageFrame);
    nameLayout->addStretch();
    m_table->setCellWidget(row.index, kColumnAction, row.nameCell);

    connect(row.replace, &QPushButton::clicked, this, [this, id] {
        const QString chord = m_replaceChord;
        dismissMessage();
        if (m_bindings && !chord.isEmpty()) {
            m_bindings->bind(id, chord, true);
        }
    });
    connect(row.dismiss, &QPushButton::clicked, this, &KeyboardPage::dismissMessage);

    row.key = new QTableWidgetItem;
    m_table->setItem(row.index, kColumnKey, row.key);

    auto* const actsOnCell = new QWidget;
    makeCellClear(actsOnCell);
    auto* const actsOnLayout = new QHBoxLayout(actsOnCell);
    actsOnLayout->setContentsMargins(2, 2, 2, 2);
    row.actsOn = new QComboBox(actsOnCell);
    row.actsOn->setObjectName(QStringLiteral("hotKeyActsOn:") + id);
    row.actsOn->setAccessibleName(QStringLiteral("Acts on"));
    row.actsOn->setStyleSheet(comboStyle());
    if (action.scope == HotKeyScope::Slice) {
        row.actsOn->addItem(QStringLiteral("Selected slice"));
        for (const QChar letter : {QLatin1Char('A'), QLatin1Char('B'), QLatin1Char('C'), QLatin1Char('D')}) {
            row.actsOn->addItem(QStringLiteral("Slice %1").arg(letter), QString(letter));
        }
        connect(row.actsOn, &QComboBox::activated, this, [this, id](int index) {
            auto* const combo = rowFor(id)->actsOn;
            const QString letter = combo->itemData(index).toString();
            if (m_bindings) {
                m_bindings->setPin(id, letter.isEmpty() ? QChar() : letter.at(0));
            }
        });
    } else {
        row.actsOn->addItem(fixedTargetName(action.scope));
    }
    actsOnLayout->addWidget(row.actsOn);
    m_table->setCellWidget(row.index, kColumnActsOn, actsOnCell);

    auto* const buttonCell = new QWidget;
    makeCellClear(buttonCell);
    auto* const buttonLayout = new QHBoxLayout(buttonCell);
    buttonLayout->setContentsMargins(2, 2, 2, 2);
    buttonLayout->setSpacing(3);
    row.set = new QPushButton(QStringLiteral("Set"), buttonCell);
    row.set->setObjectName(QStringLiteral("hotKeySet:") + id);
    row.clear = new QPushButton(QStringLiteral("Clear"), buttonCell);
    row.clear->setObjectName(QStringLiteral("hotKeyClear:") + id);
    row.toDefault = new QPushButton(QStringLiteral("Default"), buttonCell);
    row.toDefault->setObjectName(QStringLiteral("hotKeyDefault:") + id);
    for (QPushButton* const button : {row.set, row.clear, row.toDefault}) {
        button->setStyleSheet(smallButtonStyle());
        buttonLayout->addWidget(button);
    }
    buttonLayout->addStretch();
    m_table->setCellWidget(row.index, kColumnButtons, buttonCell);

    connect(row.set, &QPushButton::clicked, this, [this, id] { beginCapture(id); });
    connect(row.clear, &QPushButton::clicked, this, [this, id] {
        endCapture();
        dismissMessage();
        if (m_bindings) {
            m_bindings->clear(id);
        }
    });
    connect(row.toDefault, &QPushButton::clicked, this, [this, id] {
        endCapture();
        dismissMessage();
        if (m_bindings) {
            m_bindings->resetToDefault(id);
        }
    });
}

KeyboardPage::Row* KeyboardPage::rowFor(const QString& id)
{
    for (Row& row : m_rows) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

void KeyboardPage::refreshAll()
{
    for (Row& row : m_rows) {
        refreshRow(row);
    }
    refreshPick();
    applyFilter();
}

void KeyboardPage::refreshRow(Row& row)
{
    const HotKeyAction* const action = HotKeyActionList::find(row.id);
    const QStringList keys = m_bindings ? m_bindings->keysFor(row.id) : QStringList{};
    const QChar pin = m_bindings ? m_bindings->pinFor(row.id) : QChar();
    const QSet<QString> keySet(keys.cbegin(), keys.cend());
    const QSet<QString> defaultSet(action->defaultKeys.cbegin(), action->defaultKeys.cend());
    const bool changed = keySet != defaultSet || !pin.isNull();
    const QString reason = reasonFor(row.id);
    const bool available = reason.isEmpty();

    row.name->setEnabled(available);
    setReason(row.name, reason);

    Qt::ItemFlags keyFlags = Qt::ItemIsSelectable;
    if (available) {
        keyFlags |= Qt::ItemIsEnabled;
    }
    row.key->setFlags(keyFlags);
    row.key->setToolTip(reason);
    if (row.id == m_captureId) {
        row.key->setText(QStringLiteral("Press a key"));
        row.key->setBackground(captureColor());
        row.key->setForeground(keyTextColor());
    } else {
        row.key->setBackground(QBrush());
        if (keys.isEmpty()) {
            row.key->setText(QStringLiteral("None"));
            row.key->setForeground(noKeyColor());
        } else {
            row.key->setText(KeyboardMapWidget::displayText(keys.join(QStringLiteral(", "))));
            if (changed && available) {
                row.key->setForeground(changedKeyColor());
            } else if (available) {
                row.key->setForeground(keyTextColor());
            } else {
                row.key->setForeground(offKeyTextColor());
            }
        }
    }

    {
        const QSignalBlocker blocker(row.actsOn);
        if (action->scope == HotKeyScope::Slice) {
            row.actsOn->setCurrentIndex(pin.isNull() ? 0 : qMax(0, row.actsOn->findData(QString(pin))));
            row.actsOn->setEnabled(available);
            setReason(row.actsOn, available ? QStringLiteral("Which slice this action changes.") : reason);
        } else {
            row.actsOn->setEnabled(false);
            setReason(row.actsOn, available ? fixedTargetReason(action->scope) : reason);
        }
    }

    row.set->setEnabled(available);
    setReason(row.set, available ? QStringLiteral("Press a key to use for this action.") : reason);
    row.clear->setEnabled(available && !keys.isEmpty());
    setReason(row.clear, !available ? reason
                                    : keys.isEmpty() ? QStringLiteral("This action has no key.")
                                                     : QStringLiteral("Remove this action's keys."));
    row.toDefault->setEnabled(available && changed);
    setReason(row.toDefault, !available ? reason
                                        : changed ? QStringLiteral("Put this action back to its default.")
                                                  : QStringLiteral("This action is at its default."));
}

void KeyboardPage::refreshPick()
{
    const QString owner = m_bindings && !m_pick.isEmpty() ? m_bindings->actionForChord(m_pick) : QString();
    const QString menuItem = m_pick.isEmpty() ? QString() : HotKeyBindings::reservedBy(m_pick);
    const bool noPick = m_pick.isEmpty();

    m_pickChord->setVisible(!noPick);
    m_pickChord->setText(KeyboardMapWidget::displayText(m_pick));
    m_pickNote->setVisible(!menuItem.isEmpty());
    m_pickNote->setText(QStringLiteral("Not changeable here"));
    m_pickChange->setVisible(!owner.isEmpty());
    m_pickClear->setVisible(!owner.isEmpty());
    m_pickAssign->setVisible(!noPick && owner.isEmpty() && menuItem.isEmpty());

    if (noPick) {
        m_pickText->setText(QStringLiteral("Click a key."));
        m_pickText->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        return;
    }
    m_pickText->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; }"));
    if (!owner.isEmpty()) {
        m_pickText->setText(HotKeyActionList::find(owner)->name);
        const QString reason = reasonFor(owner);
        for (QPushButton* const button : {m_pickChange, m_pickClear}) {
            button->setEnabled(reason.isEmpty());
            setReason(button, reason);
        }
        return;
    }
    if (!menuItem.isEmpty()) {
        m_pickText->setText(QStringLiteral("%1 menu shortcut").arg(menuItem));
        return;
    }
    m_pickText->clear();
    // An action that can't be used can't take a new key either, and says why.
    if (auto* const model = qobject_cast<QStandardItemModel*>(m_pickAssign->model())) {
        for (int i = 1; i < model->rowCount(); ++i) {
            QStandardItem* const item = model->item(i);
            const QString reason = reasonFor(item->data(Qt::UserRole).toString());
            item->setEnabled(reason.isEmpty());
            item->setToolTip(reason);
        }
    }
}

void KeyboardPage::applyFilter()
{
    const QString query = m_search->text().trimmed().toLower();
    const QString group = m_groupFilter->currentData().toString();
    int shown = 0;
    for (const Row& row : m_rows) {
        const HotKeyAction* const action = HotKeyActionList::find(row.id);
        const QStringList keys = m_bindings ? m_bindings->keysFor(row.id) : QStringList{};
        const QString raw = keys.join(QLatin1Char(' '));
        const bool matchesQuery = query.isEmpty() || action->name.toLower().contains(query)
            || raw.toLower().contains(query) || KeyboardMapWidget::displayText(raw).toLower().contains(query);
        const bool matches = matchesQuery && (group.isEmpty() || action->group == group);
        m_table->setRowHidden(row.index, !matches);
        shown += matches ? 1 : 0;
    }
    m_table->setRowHidden(m_noMatchRow, shown != 0);
}

void KeyboardPage::applyEnabled(bool on)
{
    if (m_bindings) {
        const QSignalBlocker blocker(m_useShortcuts);
        m_useShortcuts->setChecked(on);
    }
    if (!on) {
        endCapture();
    }
    const bool usable = on && m_bindings;
    for (QGroupBox* const group : {m_mapGroup, m_actionsGroup}) {
        group->setEnabled(usable);
        const QString reason = !m_bindings ? noBindingsReason() : usable ? QString() : switchedOffReason();
        group->setToolTip(reason);
        group->setAccessibleDescription(reason);
    }
}

void KeyboardPage::beginCapture(const QString& id)
{
    endCapture();
    dismissMessage();
    setStatus({});
    Row* const row = rowFor(id);
    if (row == nullptr || !m_bindings) {
        return;
    }
    m_captureId = id;
    // The last filter installed runs first, so this sees each key before the
    // window's router or a menu shortcut can.
    qApp->installEventFilter(this);
    revealRow(*row);
    refreshRow(*row);
}

void KeyboardPage::endCapture()
{
    if (m_captureId.isEmpty()) {
        return;
    }
    qApp->removeEventFilter(this);
    const QString id = std::exchange(m_captureId, QString());
    if (Row* const row = rowFor(id)) {
        refreshRow(*row);
    }
}

bool KeyboardPage::eventFilter(QObject* watched, QEvent* event)
{
    if (m_captureId.isEmpty()) {
        return SetupPage::eventFilter(watched, event);
    }
    QWidget* const ours = window();
    bool inOurWindow = false;
    if (watched->isWidgetType()) {
        inOurWindow = static_cast<QWidget*>(watched)->window() == ours;
    } else if (watched->isWindowType()) {
        inOurWindow = watched == ours->windowHandle();
    }
    if (!inOurWindow) {
        return SetupPage::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::ShortcutOverride:
        // Claim it, so a menu shortcut waits and the press reaches this filter.
        event->accept();
        return true;
    case QEvent::KeyPress: {
        const auto* const key = static_cast<QKeyEvent*>(event);
        if (key->isAutoRepeat()) {
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            endCapture();
            return true;
        }
        const QString chord = KeyboardRouter::chordFromEvent(key);
        if (chord.isEmpty()) {
            return true;  // a bare modifier: keep waiting for the key it goes with
        }
        const QString id = m_captureId;
        endCapture();
        tryBind(id, chord);
        return true;
    }
    case QEvent::KeyRelease:
        return true;
    case QEvent::MouseButtonPress:
    case QEvent::WindowDeactivate:
        endCapture();
        return false;
    default:
        return SetupPage::eventFilter(watched, event);
    }
}

void KeyboardPage::tryBind(const QString& id, const QString& chord)
{
    if (!m_bindings) {
        return;
    }
    dismissMessage();
    const HotKeyBindOutcome outcome = m_bindings->bind(id, chord, false);
    switch (outcome.result) {
    case HotKeyBindResult::Bound:
        break;
    case HotKeyBindResult::Conflict:
        showMessage(id, KeyboardMapWidget::displayText(outcome.reason), chord);
        break;
    case HotKeyBindResult::RefusedTransmitPlainKey:
    case HotKeyBindResult::RefusedLatchedOnSpace:
    case HotKeyBindResult::RefusedReserved:
    case HotKeyBindResult::RefusedInvalid:
        showMessage(id, KeyboardMapWidget::displayText(outcome.reason), {});
        break;
    }
}

void KeyboardPage::showMessage(const QString& id, const QString& text, const QString& replaceChord)
{
    Row* const row = rowFor(id);
    if (row == nullptr) {
        return;
    }
    m_messageId = id;
    m_replaceChord = replaceChord;
    row->message->setText(text);
    row->replace->setVisible(!replaceChord.isEmpty());
    row->dismiss->setText(replaceChord.isEmpty() ? QStringLiteral("OK") : QStringLiteral("Cancel"));
    row->messageFrame->setVisible(true);
    fitMessageRow();
    revealRow(*row);
}

void KeyboardPage::dismissMessage()
{
    if (m_messageId.isEmpty()) {
        return;
    }
    const QString id = std::exchange(m_messageId, QString());
    m_replaceChord.clear();
    if (Row* const row = rowFor(id)) {
        row->messageFrame->setVisible(false);
        m_table->setRowHeight(row->index, kRowHeight);
    }
}

void KeyboardPage::fitMessageRow()
{
    if (m_messageId.isEmpty()) {
        return;
    }
    if (Row* const row = rowFor(m_messageId)) {
        const int width = m_table->columnWidth(kColumnAction);
        QLayout* const layout = row->nameCell->layout();
        const int height = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width)
                                                       : layout->totalSizeHint().height();
        m_table->setRowHeight(row->index, qMax(kRowHeight, height));
    }
}

// A row hidden by the search can't show its message or its "Press a key".
void KeyboardPage::revealRow(Row& row)
{
    if (m_table->isRowHidden(row.index)) {
        {
            const QSignalBlocker searchBlocker(m_search);
            const QSignalBlocker groupBlocker(m_groupFilter);
            m_search->clear();
            m_groupFilter->setCurrentIndex(0);
        }
        applyFilter();
    }
    m_table->selectRow(row.index);
    m_table->scrollToItem(row.key);
}

void KeyboardPage::onKeyClicked(const QString& chord)
{
    endCapture();
    m_pick = chord;
    m_map->setPickedChord(chord);
    const QString owner = m_bindings ? m_bindings->actionForChord(chord) : QString();
    if (Row* const row = owner.isEmpty() ? nullptr : rowFor(owner)) {
        if (!m_table->isRowHidden(row->index)) {
            m_table->selectRow(row->index);
            m_table->scrollToItem(row->key);
        }
    } else {
        m_table->clearSelection();
    }
    refreshPick();
}

void KeyboardPage::onImportClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Shortcuts"), QDir::homePath(),
        QStringLiteral("Text files (*.txt);;All files (*)"));
    if (!path.isEmpty()) {
        importFromFile(path);
    }
}

void KeyboardPage::onExportClicked()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Shortcuts"),
        QDir::home().filePath(QStringLiteral("NereusSDR-shortcuts.txt")),
        QStringLiteral("Text files (*.txt);;All files (*)"));
    if (!path.isEmpty()) {
        exportToFile(path);
    }
}

bool KeyboardPage::importFromFile(const QString& path)
{
    if (!m_bindings) {
        return false;
    }
    endCapture();
    dismissMessage();
    m_skipped->clear();
    m_skipped->setVisible(false);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Could not read %1.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    if (file.size() > kMostImportBytes) {
        setStatus(QStringLiteral("%1 is too large to be a shortcuts file.").arg(QDir::toNativeSeparators(path)));
        return false;
    }

    const QStringList skipped = m_bindings->importText(QString::fromUtf8(file.readAll()));
    if (skipped.isEmpty()) {
        setStatus(QStringLiteral("Shortcuts imported."));
        return true;
    }
    setStatus(skipped.size() == 1 ? QStringLiteral("Imported the good lines. 1 line was skipped:")
                                  : QStringLiteral("Imported the good lines. %1 lines were skipped:").arg(skipped.size()));
    m_skipped->setPlainText(KeyboardMapWidget::displayText(skipped.join(QLatin1Char('\n'))));
    m_skipped->setVisible(true);
    return true;
}

bool KeyboardPage::exportToFile(const QString& path)
{
    if (!m_bindings) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(m_bindings->exportText().toUtf8()) < 0 || !file.commit()) {
        setStatus(QStringLiteral("Could not write %1.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    setStatus(QStringLiteral("Shortcuts saved to %1.").arg(QDir::toNativeSeparators(path)));
    return true;
}

void KeyboardPage::setStatus(const QString& text)
{
    m_status->setText(text);
}

void KeyboardPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    refreshAll();
}

void KeyboardPage::hideEvent(QHideEvent* event)
{
    endCapture();
    SetupPage::hideEvent(event);
}

} // namespace NereusSDR
