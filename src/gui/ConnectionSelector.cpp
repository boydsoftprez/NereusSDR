// =================================================================
// src/gui/ConnectionSelector.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
// =================================================================

#include "gui/ConnectionSelector.h"

#include "core/security/PairingCode.h"
#include "core/session/StationPairingClient.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {
namespace {

constexpr int kKeyRole = Qt::UserRole;
constexpr int kKindRole = Qt::UserRole + 1;

void configurePlainTextLabel(QLabel* label)
{
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
}

} // namespace

ConnectionSelector::ConnectionSelector(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Connections"));
    setModal(false);
    setWindowModality(Qt::NonModal);
    setObjectName(QStringLiteral("connectionSelector"));

    auto* layout = new QVBoxLayout(this);

    m_noticeLabel = new QLabel(this);
    m_noticeLabel->setObjectName(QStringLiteral("connectionSelectorNotice"));
    configurePlainTextLabel(m_noticeLabel);
    m_noticeLabel->setVisible(false);
    layout->addWidget(m_noticeLabel);

    m_discoveryStatusLabel = new QLabel(this);
    m_discoveryStatusLabel->setObjectName(QStringLiteral("connectionSelectorDiscoveryStatus"));
    configurePlainTextLabel(m_discoveryStatusLabel);
    layout->addWidget(m_discoveryStatusLabel);

    m_targetTree = new QTreeWidget(this);
    m_targetTree->setObjectName(QStringLiteral("connectionSelectorTargets"));
    m_targetTree->setColumnCount(4);
    m_targetTree->setHeaderLabels({tr("Name"), tr("Radio"), tr("Address"), tr("Status")});
    m_targetTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_targetTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_targetTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_targetTree->setRootIsDecorated(false);
    m_targetTree->setUniformRowHeights(true);
    m_targetTree->header()->setStretchLastSection(true);
    m_targetTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_targetTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_targetTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_targetTree->setMinimumHeight(240);
    layout->addWidget(m_targetTree, 1);

    auto* currentGroup = new QGroupBox(tr("Current connection"), this);
    auto* currentLayout = new QVBoxLayout(currentGroup);
    // R-R3-17: the connection text changes while the window is open (a retry
    // adds lines, a long address wraps). It scrolls inside a fixed area so
    // the window never grows to fit it. Room for the summary plus four
    // detail lines, which covers the retry text without a scroll bar.
    auto* currentScroll = new QScrollArea(currentGroup);
    currentScroll->setObjectName(QStringLiteral("connectionSelectorCurrentScroll"));
    currentScroll->setFrameShape(QFrame::NoFrame);
    currentScroll->setWidgetResizable(true);
    currentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* currentText = new QWidget(currentScroll);
    auto* currentTextLayout = new QVBoxLayout(currentText);
    currentTextLayout->setContentsMargins(0, 0, 0, 0);
    m_currentSummaryLabel = new QLabel(currentText);
    m_currentSummaryLabel->setObjectName(QStringLiteral("connectionSelectorCurrentSummary"));
    configurePlainTextLabel(m_currentSummaryLabel);
    m_currentDetailsLabel = new QLabel(currentText);
    m_currentDetailsLabel->setObjectName(QStringLiteral("connectionSelectorCurrentDetails"));
    configurePlainTextLabel(m_currentDetailsLabel);
    m_currentDetailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    currentTextLayout->addWidget(m_currentSummaryLabel);
    currentTextLayout->addWidget(m_currentDetailsLabel);
    currentTextLayout->addStretch();
    currentScroll->setWidget(currentText);
    currentScroll->setFixedHeight(5 * m_currentDetailsLabel->fontMetrics().lineSpacing()
                                  + currentTextLayout->spacing());
    currentLayout->addWidget(currentScroll);
    layout->addWidget(currentGroup);

    // R-R3-17: the row actions show and hide with the selected row. Measured
    // with all ten shown (and the wider of the Disconnect captions), the row
    // keeps that width as its minimum, so showing a hidden button never
    // raises the window's minimum width and makes Qt enlarge the window.
    auto* actionRow = new QWidget(this);
    actionRow->setObjectName(QStringLiteral("connectionSelectorActions"));
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    // iPhone app Task 18: "Type an address" is the Core setup editor the
    // Add Core button opened (a Core's address, and for a Core from before
    // paired devices its token and certificate fingerprint); "Add a Core by
    // code" pairs with a Core by the code it shows.
    m_addCoreButton = makeButton(tr("Type an address…"), QStringLiteral("connectionSelectorAddCore"));
    m_addByCodeButton = makeButton(tr("Add a Core by code…"),
                                   QStringLiteral("connectionSelectorAddByCode"));
    m_addRadioButton = makeButton(tr("Add Radio…"), QStringLiteral("connectionSelectorAddRadio"));
    m_scanButton = makeButton(tr("Scan"), QStringLiteral("connectionSelectorScan"));
    m_manageCoreButton = makeButton(tr("Manage…"), QStringLiteral("connectionSelectorManageCore"));
    m_editButton = makeButton(tr("Edit…"), QStringLiteral("connectionSelectorEdit"));
    m_forgetButton = makeButton(tr("Forget…"), QStringLiteral("connectionSelectorForget"));
    m_detailsButton = makeButton(tr("Details"), QStringLiteral("connectionSelectorDetails"));
    m_disconnectButton = makeButton(tr("Disconnect"), QStringLiteral("connectionSelectorDisconnect"));
    m_connectButton = makeButton(tr("Connect"), QStringLiteral("connectionSelectorConnect"));
    auto* closeButton = makeButton(tr("Close"), QStringLiteral("connectionSelectorClose"));

    actionLayout->addWidget(m_addByCodeButton);
    actionLayout->addWidget(m_addCoreButton);
    actionLayout->addWidget(m_addRadioButton);
    actionLayout->addWidget(m_scanButton);
    actionLayout->addWidget(m_manageCoreButton);
    actionLayout->addWidget(m_editButton);
    actionLayout->addWidget(m_forgetButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_detailsButton);
    actionLayout->addWidget(m_disconnectButton);
    actionLayout->addWidget(m_connectButton);
    actionLayout->addWidget(closeButton);
    layout->addWidget(actionRow);

    // Every button is still unhidden here (setTargets() below applies the
    // first selection), so the layout's size hint covers the full row.
    // The Connect button reads Pair for a Core that takes new devices.
    int fullRowWidth = 0;
    for (const QString& connectCaption : {tr("Connect"), tr("Pair")}) {
        m_connectButton->setText(connectCaption);
        for (const QString& caption : {tr("Cancel retry"), tr("Disconnect")}) {
            m_disconnectButton->setText(caption);
            actionLayout->invalidate();
            fullRowWidth = std::max(fullRowWidth, actionLayout->sizeHint().width());
        }
    }
    actionRow->setMinimumWidth(fullRowWidth);

    connect(m_targetTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem*, QTreeWidgetItem*) { updateActions(); });
    connect(m_addCoreButton, &QPushButton::clicked, this, &ConnectionSelector::addCoreRequested);
    connect(m_addByCodeButton, &QPushButton::clicked, this,
            &ConnectionSelector::addByCodeRequested);
    connect(m_addRadioButton, &QPushButton::clicked, this, &ConnectionSelector::addRadioRequested);
    connect(m_scanButton, &QPushButton::clicked, this, &ConnectionSelector::scanRequested);
    connect(m_manageCoreButton, &QPushButton::clicked, this, [this] {
        if (const ConnectionTargetRow* target = selectedTarget(); target != nullptr
            && m_coreManagementAvailable && target->kind == ConnectionTargetKind::SavedCore) {
            emit manageCoreRequested(target->key);
        }
    });
    connect(m_editButton, &QPushButton::clicked, this, [this] {
        if (const ConnectionTargetRow* target = selectedTarget(); target != nullptr) {
            emit editRequested(target->key);
        }
    });
    connect(m_forgetButton, &QPushButton::clicked, this, [this] {
        if (const ConnectionTargetRow* target = selectedTarget(); target != nullptr) {
            emit forgetRequested(target->key);
        }
    });
    connect(m_detailsButton, &QPushButton::clicked, this, [this] {
        if (const ConnectionTargetRow* target = selectedTarget(); target != nullptr) {
            emit detailsRequested(target->key);
        }
    });
    connect(m_disconnectButton, &QPushButton::clicked, this,
            &ConnectionSelector::disconnectRequested);
    connect(m_connectButton, &QPushButton::clicked, this, [this] {
        const ConnectionTargetRow* target = selectedTarget();
        if (target == nullptr) {
            return;
        }
        if (target->pairable) {
            emit pairRequested(target->key);
        } else if (target->connectable) {
            emit connectRequested(target->key);
        }
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    setTargets({});
    setCurrentConnection({}, {}, false, false);
    // Open at least as wide as the full action row needs (native button
    // metrics can exceed 820), never narrower.
    resize(std::max(820, minimumSizeHint().width()), std::max(540, minimumSizeHint().height()));
}

void ConnectionSelector::setTargets(const QList<ConnectionTargetRow>& targets)
{
    const QString previousKey = selectedKey();
    m_targets = targets;

    QList<ConnectionTargetRow> localRadios;
    QList<ConnectionTargetRow> lanCores;
    QList<ConnectionTargetRow> savedCores;
    for (const ConnectionTargetRow& target : m_targets) {
        switch (target.kind) {
        case ConnectionTargetKind::LocalRadio: localRadios.append(target); break;
        case ConnectionTargetKind::LanCore: lanCores.append(target); break;
        case ConnectionTargetKind::SavedCore: savedCores.append(target); break;
        }
    }

    const bool structureChanges =
        !groupStructureMatches(ConnectionTargetKind::LocalRadio, localRadios)
        || !groupStructureMatches(ConnectionTargetKind::LanCore, lanCores)
        || !groupStructureMatches(ConnectionTargetKind::SavedCore, savedCores);
#if defined(Q_OS_MAC)
    if (structureChanges && QGuiApplication::platformName() == QStringLiteral("cocoa")
        && qVersion() == QStringLiteral("6.11.0")) {
        // Qt 6.11 Cocoa releases real cell interfaces when old native rows
        // expire (qcocoaaccessibilityelement.mm:219-226,257-267,342-362).
        // QAccessibleTable retains their IDs and dereferences them during
        // RowsRemoved/RowsInserted (itemviews.cpp:645-741). Clear that cache
        // through its public API before changing rows. This does not reset
        // the item model or replace surviving items/persistent indexes.
        QAccessibleInterface* accessible = QAccessible::queryAccessibleInterface(m_targetTree);
        if (accessible != nullptr && accessible->tableInterface() != nullptr) {
            QAccessibleTableModelChangeEvent reset(
                m_targetTree, QAccessibleTableModelChangeEvent::ModelReset);
            accessible->tableInterface()->modelChange(&reset);
        }
    }
#endif
    if (structureChanges && !previousKey.isEmpty()) {
        // Clear the selection before removing rows. Qt's macOS accessibility
        // bridge keeps separate table and selection caches; deleting the
        // selected interface during a model reset can leave it with a stale
        // pointer while AppKit is enumerating accessibilitySelectedChildren.
        m_targetTree->setCurrentItem(nullptr);
    }
    addGroup(tr("Radios on this network"), ConnectionTargetKind::LocalRadio,
             tr("No radios found on this network."), localRadios);
    addGroup(tr("Cores on this network"), ConnectionTargetKind::LanCore,
             tr("No Cores found on this network."), lanCores);
    addGroup(tr("Your Cores"), ConnectionTargetKind::SavedCore,
             tr("No saved Cores."), savedCores);
    setSelectedKey(previousKey);
    updateActions();
}

void ConnectionSelector::setCurrentConnection(const QString& summary, const QString& details,
                                               bool canDisconnect, bool retrying)
{
    m_currentSummaryLabel->setText(summary);
    m_currentDetailsLabel->setText(details);
    m_canDisconnect = canDisconnect;
    m_retrying = retrying;
    updateActions();
}

void ConnectionSelector::setDiscoveryStatus(const QString& text)
{
    m_discoveryStatusLabel->setText(text);
}

void ConnectionSelector::setNotice(const QString& notice)
{
    m_noticeLabel->setText(notice);
    m_noticeLabel->setVisible(!notice.isEmpty());
}

QString ConnectionSelector::selectedKey() const
{
    const QTreeWidgetItem* item = m_targetTree->currentItem();
    return item == nullptr ? QString{} : item->data(0, kKeyRole).toString();
}

void ConnectionSelector::setSelectedKey(const QString& key)
{
    if (key.isEmpty()) {
        m_targetTree->setCurrentItem(nullptr);
        return;
    }

    for (int groupIndex = 0; groupIndex < m_targetTree->topLevelItemCount(); ++groupIndex) {
        QTreeWidgetItem* group = m_targetTree->topLevelItem(groupIndex);
        for (int rowIndex = 0; rowIndex < group->childCount(); ++rowIndex) {
            QTreeWidgetItem* row = group->child(rowIndex);
            if (row->data(0, kKeyRole).toString() == key) {
                m_targetTree->setCurrentItem(row);
                return;
            }
        }
    }
    m_targetTree->setCurrentItem(nullptr);
}

void ConnectionSelector::addGroup(const QString& title, ConnectionTargetKind kind,
                                  const QString& emptyText,
                                  const QList<ConnectionTargetRow>& targets)
{
    QTreeWidgetItem* group = groupForKind(kind);
    if (group == nullptr) {
        group = new QTreeWidgetItem(m_targetTree, {title});
        group->setData(0, kKindRole, static_cast<int>(kind));
        group->setFirstColumnSpanned(true);
        group->setFlags(Qt::ItemIsEnabled);
    } else if (group->text(0) != title) {
        group->setText(0, title);
    }

    if (targets.isEmpty()) {
        QTreeWidgetItem* empty = nullptr;
        if (group->childCount() == 1 && group->child(0)->data(0, kKeyRole).toString().isEmpty()) {
            empty = group->child(0);
            if (empty->text(0) != emptyText) {
                empty->setText(0, emptyText);
            }
        } else {
            while (group->childCount() > 0) {
                delete group->takeChild(group->childCount() - 1);
            }
            empty = new QTreeWidgetItem(group, {emptyText});
            empty->setFirstColumnSpanned(true);
            empty->setFlags(Qt::ItemIsEnabled);
        }
        group->setExpanded(true);
        return;
    }

    QList<QString> unmatchedKeys;
    unmatchedKeys.reserve(targets.size());
    for (const ConnectionTargetRow& target : targets) {
        unmatchedKeys.append(target.key);
    }
    for (int childIndex = group->childCount() - 1; childIndex >= 0; --childIndex) {
        const QString key = group->child(childIndex)->data(0, kKeyRole).toString();
        const int match = unmatchedKeys.lastIndexOf(key);
        if (key.isEmpty() || match < 0) {
            delete group->takeChild(childIndex);
        } else {
            unmatchedKeys.removeAt(match);
        }
    }

    for (int targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
        const ConnectionTargetRow& target = targets.at(targetIndex);
        QTreeWidgetItem* row = nullptr;
        if (targetIndex < group->childCount()
            && group->child(targetIndex)->data(0, kKeyRole).toString() == target.key) {
            row = group->child(targetIndex);
        } else {
            for (int childIndex = targetIndex + 1; childIndex < group->childCount(); ++childIndex) {
                if (group->child(childIndex)->data(0, kKeyRole).toString() == target.key) {
                    row = group->takeChild(childIndex);
                    group->insertChild(targetIndex, row);
                    break;
                }
            }
            if (row == nullptr) {
                row = new QTreeWidgetItem();
                group->insertChild(targetIndex, row);
            }
        }

        const QStringList text{target.name, target.radioText, target.address, target.state};
        for (int column = 0; column < text.size(); ++column) {
            if (row->text(column) != text.at(column)) {
                row->setText(column, text.at(column));
            }
        }
        if (row->data(0, kKeyRole).toString() != target.key) {
            row->setData(0, kKeyRole, target.key);
        }
        if (row->data(0, kKindRole).toInt() != static_cast<int>(kind)) {
            row->setData(0, kKindRole, static_cast<int>(kind));
        }
        const Qt::ItemFlags rowFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (row->flags() != rowFlags) {
            row->setFlags(rowFlags);
        }
    }
    while (group->childCount() > targets.size()) {
        delete group->takeChild(group->childCount() - 1);
    }
    group->setExpanded(true);
}

QTreeWidgetItem* ConnectionSelector::groupForKind(ConnectionTargetKind kind) const
{
    for (int index = 0; index < m_targetTree->topLevelItemCount(); ++index) {
        QTreeWidgetItem* group = m_targetTree->topLevelItem(index);
        if (group->data(0, kKindRole).toInt() == static_cast<int>(kind)) {
            return group;
        }
    }
    return nullptr;
}

bool ConnectionSelector::groupStructureMatches(
    ConnectionTargetKind kind, const QList<ConnectionTargetRow>& targets) const
{
    const QTreeWidgetItem* group = groupForKind(kind);
    if (group == nullptr) {
        return false;
    }
    if (targets.isEmpty()) {
        return group->childCount() == 1
            && group->child(0)->data(0, kKeyRole).toString().isEmpty();
    }
    if (group->childCount() != targets.size()) {
        return false;
    }
    for (int index = 0; index < targets.size(); ++index) {
        if (group->child(index)->data(0, kKeyRole).toString() != targets.at(index).key) {
            return false;
        }
    }
    return true;
}

const ConnectionTargetRow* ConnectionSelector::selectedTarget() const
{
    const QString key = selectedKey();
    if (key.isEmpty()) {
        return nullptr;
    }
    for (const ConnectionTargetRow& target : m_targets) {
        if (target.key == key) {
            return &target;
        }
    }
    return nullptr;
}

void ConnectionSelector::setCoreManagementAvailable(bool available)
{
    m_coreManagementAvailable = available;
    updateActions();
}

void ConnectionSelector::updateActions()
{
    const ConnectionTargetRow* target = selectedTarget();
    const bool hasTarget = target != nullptr;
    const bool canPair = hasTarget && target->pairable;
    const bool canConnect = hasTarget && (target->connectable || canPair);
    const bool canEdit = hasTarget && target->editable;
    const bool canForget = hasTarget && target->forgettable;
    const bool canManage = hasTarget && m_coreManagementAvailable
        && target->kind == ConnectionTargetKind::SavedCore;
    m_manageCoreButton->setVisible(canManage);
    m_manageCoreButton->setEnabled(canManage);
    m_connectButton->setText(canPair ? tr("Pair") : tr("Connect"));
    m_connectButton->setVisible(canConnect);
    m_connectButton->setEnabled(canConnect);
    m_editButton->setVisible(canEdit);
    m_editButton->setEnabled(canEdit);
    m_forgetButton->setVisible(canForget);
    m_forgetButton->setEnabled(canForget);
    m_detailsButton->setVisible(hasTarget);
    m_detailsButton->setEnabled(hasTarget);
    m_disconnectButton->setText(m_retrying ? tr("Cancel retry") : tr("Disconnect"));
    const bool canDisconnect = m_canDisconnect || m_retrying;
    m_disconnectButton->setVisible(canDisconnect);
    m_disconnectButton->setEnabled(canDisconnect);
}

QPushButton* ConnectionSelector::makeButton(const QString& text, const QString& objectName)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName(objectName);
    button->setAutoDefault(false);
    return button;
}

AddCoreByCodeDialog::AddCoreByCodeDialog(const QString& address, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add a Core by code"));
    setObjectName(QStringLiteral("addCoreByCode"));

    auto* layout = new QVBoxLayout(this);
    // Part C fix wave (R2-M2): the places that show the code. A headless
    // Core has no screen; RemoteStationPage now shows the code while its
    // pairing window is open, and the status page and CLI also show it.
    auto* explanation = new QLabel(
        tr("Enter the pairing code from the Core's status page or Remote Access page. "
           "For a headless Core, run nereusd pairing show on its computer."),
        this);
    explanation->setObjectName(QStringLiteral("addCoreByCodeExplanation"));
    configurePlainTextLabel(explanation);
    layout->addWidget(explanation);

    auto* form = new QFormLayout();
    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setObjectName(QStringLiteral("addCoreByCodeCode"));
    m_codeEdit->setPlaceholderText(tr("7-anvil-harbor"));
    m_codeEdit->setMaxLength(64);
    form->addRow(tr("Pairing code:"), m_codeEdit);
    layout->addLayout(form);

    auto* hint = new QLabel(tr("The app will find the Core and connect automatically."), this);
    configurePlainTextLabel(hint);
    layout->addWidget(hint);

    auto* addressOptions = new QPushButton(tr("Use a Core address instead…"), this);
    addressOptions->setObjectName(QStringLiteral("addCoreByCodeAddressOptions"));
    addressOptions->setCheckable(true);
    addressOptions->setAutoDefault(false);
    layout->addWidget(addressOptions);
    m_addressGroup = new QWidget(this);
    auto* addressForm = new QFormLayout(m_addressGroup);
    m_addressEdit = new QLineEdit(address, m_addressGroup);
    m_addressEdit->setObjectName(QStringLiteral("addCoreByCodeAddress"));
    m_addressEdit->setPlaceholderText(tr("shack-core.local, 192.168.1.20 or [2001:db8::20]"));
    m_addressEdit->setMaxLength(512);
    addressForm->addRow(tr("Core address:"), m_addressEdit);
    layout->addWidget(m_addressGroup);
    addressOptions->setChecked(!address.isEmpty());
    m_addressGroup->setVisible(!address.isEmpty());
    connect(addressOptions, &QPushButton::toggled, this, [this](bool shown) {
        m_addressGroup->setVisible(shown);
        if (!shown) { m_addressEdit->clear(); }
    });

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("addCoreByCodeError"));
    configurePlainTextLabel(m_errorLabel);
    m_errorLabel->setVisible(false);
    layout->addWidget(m_errorLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* pairButton = buttons->addButton(tr("Pair and connect"), QDialogButtonBox::AcceptRole);
    auto* cancelButton = buttons->button(QDialogButtonBox::Cancel);
    pairButton->setObjectName(QStringLiteral("addCoreByCodePair"));
    cancelButton->setObjectName(QStringLiteral("addCoreByCodeCancel"));
    pairButton->setAutoDefault(false);
    cancelButton->setAutoDefault(false);
    layout->addWidget(buttons);
    connect(pairButton, &QPushButton::clicked, this, [this] {
        if (validate()) {
            accept();
        }
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    if (!address.isEmpty()) {
        m_codeEdit->setFocus();
    }
    resize(480, sizeHint().height());
}

QString AddCoreByCodeDialog::code() const
{
    return m_codeEdit->text();
}

bool AddCoreByCodeDialog::validate()
{
    // Checked here, before anything is sent: a mistyped word would
    // otherwise burn the Core's code.
    if (PairingCode::normalise(m_codeEdit->text()).isEmpty()) {
        m_errorLabel->setText(tr("Check the code. It is a number and two words, such as "
                                 "7-anvil-harbor."));
        m_errorLabel->setVisible(true);
        return false;
    }
    m_host.clear();
    m_port = 0;
    if (!m_addressEdit->text().trimmed().isEmpty()) {
        QString host;
        quint16 port = 0;
        if (!StationPairingClient::parseAddress(m_addressEdit->text(), &host, &port)) {
            m_errorLabel->setText(tr("Enter a valid Core address or leave it blank to find the Core automatically."));
            m_errorLabel->setVisible(true);
            return false;
        }
        m_host = host;
        m_port = port;
    }
    m_errorLabel->clear();
    m_errorLabel->setVisible(false);
    return true;
}

} // namespace NereusSDR
