// no-port-check: NereusSDR-original approved native Core Settings flow.
// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CoresSetupPage.h"
#include "ThisCorePage.h"
#include "core/session/RemoteDevicesState.h"
#include "core/security/StationLabel.h"
#include "models/RadioModel.h"
#include <QGridLayout>
#include <QApplication>
#include <QHideEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QUrl>

namespace NereusSDR {
namespace {
class WrappingLabel final : public QLabel {
public:
    WrappingLabel(const QString& value, QWidget* parent) : QLabel(value, parent) {}
    QSize minimumSizeHint() const override { return QSize(0, fontMetrics().lineSpacing()); }
};
class CoreChoiceButton final : public QPushButton {
public:
    explicit CoreChoiceButton(QWidget* parent) : QPushButton(parent) {
        setAutoDefault(false);
        QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }
    QSize sizeHint() const override { return layout() ? layout()->sizeHint() : QPushButton::sizeHint(); }
    QSize minimumSizeHint() const override { return QSize(0, 40); }
    int heightForWidth(int width) const override {
        return layout() ? qMax(40, layout()->totalHeightForWidth(width)) : 40;
    }
};
QLabel* text(const QString& value, QWidget* parent, bool muted = false)
{
    auto* label = new WrappingLabel(value, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    label->setStyleSheet(muted ? "color:#8090a0; background:transparent;"
                              : "color:#c8d8e8; background:transparent;");
    return label;
}
QPushButton* button(const QString& caption, QWidget* parent, const char* name = "")
{
    auto* result = new QPushButton(caption, parent);
    result->setObjectName(QString::fromLatin1(name));
    result->setAutoDefault(false);
    return result;
}
QVBoxLayout* vertical(QWidget* widget)
{
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    return layout;
}
QGroupBox* group(const QString& caption, QWidget* parent)
{
    auto* box = new QGroupBox(caption, parent);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(8, 16, 8, 8);
    layout->setSpacing(6);
    return box;
}
QString addressDisplay(const QString& address)
{
    const QUrl url(address);
    const QString host = url.host();
    return QStringLiteral("%1:%2").arg(host.contains(':') ? '[' + host + ']' : host)
        .arg(url.port());
}
QString savedIdentifier(const SavedCoreTarget& target)
{
    if (!target.connection.rendezvousId.isEmpty()) {
        return CoresSetupPage::tr("Core ID: %1").arg(target.connection.rendezvousId);
    }
    const QUrl url(target.connection.url);
    if (url.isValid() && !url.host().isEmpty()) {
        const QString host = url.host().contains(':') ? '[' + url.host() + ']' : url.host();
        return CoresSetupPage::tr("Configured address: %1").arg(url.port() > 0
            ? host + ':' + QString::number(url.port()) : host);
    }
    return target.connection.identityFingerprint.isEmpty()
        ? CoresSetupPage::tr("Saved Core ID: %1").arg(target.id)
        : CoresSetupPage::tr("Core identity: %1").arg(
            QString::fromLatin1(target.connection.identityFingerprint.toHex().left(12)));
}
void clearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        if (QLayout* child = item->layout()) {
            clearLayout(child);
        }
        delete item;
    }
}
}

CoresSetupPage::CoresSetupPage(CoreTargetStore* store, RadioModel* model, QWidget* parent,
                             CoreAddressProbe::RungFactory factory, int deadlineMs)
    : SetupPage(tr("Cores"), parent), m_store(store), m_radioModel(model)
{
    setObjectName(QStringLiteral("coresSetupPage"));
    setStyleSheet(QStringLiteral(
        "QWidget { background:#0f0f1a; color:#c8d8e8; font-size:12px; }"
        "QLabel { background:transparent; font-weight:400; }"
        "QGroupBox { border:1px solid #304050; border-radius:4px; margin-top:8px; "
        "color:#8aa8c0; font-weight:600; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }"
        "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #233347,stop:1 #1a2a3a);"
        "border:1px solid #304050; border-radius:3px; padding:3px 10px; min-height:18px; font-weight:400; }"
        "QPushButton:hover:enabled { background:#203040; }"
        "QPushButton:pressed:enabled { background:#00b4d8; color:#0f0f1a; }"
        "QPushButton:disabled { background:#1a1a2a; color:#556070; border-color:#2a3040; }"
        "QPushButton:focus, QLineEdit:focus, QSpinBox:focus { border:1px solid #00b4d8; }"
        "QLineEdit,QSpinBox { background:#1a2a3a; border:1px solid #304050; border-radius:3px; "
        "padding:2px 6px; min-height:19px; }"
        "QTabBar::tab { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #3f444d,stop:1 #292f39);"
        "border:1px solid #59616a; border-top-left-radius:3px; border-top-right-radius:3px; padding:4px 11px; }"
        "QTabBar::tab:selected { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #535966,stop:1 #3b424e);"
        "border-bottom-color:#3b424e; }"));
    QScrollArea* scroll = findChild<QScrollArea*>();
    // SetupPage's content-local QWidget background otherwise overrides this
    // page's more distant button/input skins, including disabled backgrounds.
    scroll->widget()->setStyleSheet(QString());
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->widget()->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QVBoxLayout* layout = contentLayout();
    delete layout->takeAt(0); // Replace SetupPage's trailing spacer with one after content.
    layout->setSpacing(6);
    m_coreList = new QWidget(this);
    m_coreList->setObjectName(QStringLiteral("coreSettingsCoreList"));
    m_coreList->setStyleSheet(QStringLiteral("#coreSettingsCoreList { border:1px solid #304050; }"));
    m_coreGrid = new QGridLayout(m_coreList);
    m_coreGrid->setContentsMargins(1, 1, 1, 1);
    m_coreGrid->setSpacing(0);
    layout->addWidget(m_coreList);
    layout->addWidget(text(tr("Inspecting a saved Core does not change this window’s connection."), this, true));
    m_heading = text({}, this);
    m_heading->setObjectName(QStringLiteral("inspectedCoreName"));
    m_heading->setStyleSheet(QStringLiteral("font-size:14px; font-weight:600; margin-top:2px;"));
    layout->addWidget(m_heading);
    m_tabs = new QTabBar(this);
    m_tabs->setObjectName(QStringLiteral("coreSettingsTabs"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    for (const QString& name : {tr("Overview"), tr("Addresses"), tr("Radio"), tr("Devices")}) {
        m_tabs->addTab(name);
    }
    auto* tabsContainer = new QWidget(this);
    tabsContainer->setObjectName(QStringLiteral("coreSettingsTabRow"));
    tabsContainer->setStyleSheet(QStringLiteral("#coreSettingsTabRow { border-bottom:1px solid #53606d; }"));
    auto* tabRow = new QHBoxLayout(tabsContainer);
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(0);
    tabRow->addWidget(m_tabs, 0, Qt::AlignLeft);
    tabRow->addStretch();
    layout->addWidget(tabsContainer);
    m_panels = new QStackedWidget(this);
    // A stacked widget normally asks for the tallest hidden page. Only the
    // active panel contributes to scroll geometry; forms never widen Settings.
    m_panels->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_panels);
    auto* overview = new QWidget(m_panels);
    auto* overviewLayout = vertical(overview);
    m_panels->addWidget(overview);
    auto* nameGroup = group(tr("Core name"), overview);
    overviewLayout->addWidget(nameGroup);
    auto* nameLayout = qobject_cast<QVBoxLayout*>(nameGroup->layout());
    m_nameDisplay = new QWidget(nameGroup);
    auto* nameRow = new QHBoxLayout(m_nameDisplay);
    nameRow->setContentsMargins(0, 0, 0, 0);
    auto* nameCaption = text(tr("Core name"), m_nameDisplay, true);
    nameCaption->setFixedWidth(142);
    nameCaption->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    nameRow->addWidget(nameCaption);
    m_name = text({}, m_nameDisplay);
    m_name->setObjectName(QStringLiteral("actualCoreName"));
    nameRow->addWidget(m_name, 1);
    m_rename = button(tr("Rename Core…"), m_nameDisplay, "renameCore");
    nameRow->addWidget(m_rename);
    nameLayout->addWidget(m_nameDisplay);
    m_nameEditor = new QWidget(nameGroup);
    auto* editorLayout = vertical(m_nameEditor);
    editorLayout->addWidget(text(tr("Core name"), m_nameEditor));
    auto* editRow = new QHBoxLayout;
    m_nameInput = new QLineEdit(m_nameEditor);
    m_nameInput->setObjectName(QStringLiteral("coreNameInput"));
    m_nameInput->setMaxLength(65);
    m_nameInput->setAccessibleName(tr("Core name"));
    editRow->addWidget(m_nameInput, 1);
    m_nameSave = button(tr("Save"), m_nameEditor, "saveCoreName");
    editRow->addWidget(m_nameSave);
    auto* cancelName = button(tr("Cancel"), m_nameEditor, "cancelCoreName");
    editRow->addWidget(cancelName);
    editorLayout->addLayout(editRow);
    editorLayout->addWidget(text(tr("Your callsign, then / and a name, like KG4VCF/shack. The callsign can use letters, digits and / (up to 32 characters); the optional name can use letters, digits, dashes or underscores (up to 32)."), m_nameEditor, true));
    m_nameError = text({}, m_nameEditor);
    m_nameError->setStyleSheet(QStringLiteral("color:#ffa3a3;"));
    editorLayout->addWidget(m_nameError);
    m_nameProgress = text({}, m_nameEditor);
    editorLayout->addWidget(m_nameProgress);
    nameLayout->addWidget(m_nameEditor);
    m_nameEditor->hide();
    nameLayout->addWidget(text(tr("Changes the name on the Core for all devices."), nameGroup));
    m_nameReason = text({}, nameGroup, true);
    nameLayout->addWidget(m_nameReason);
    auto* connection = group(tr("Connection in this window"), overview);
    auto* facts = new QGridLayout;
    facts->setHorizontalSpacing(8);
    facts->setVerticalSpacing(4);
    int row = 0;
    for (const QString& caption : {tr("Status"), tr("Reached through"), tr("Controls"),
                                  tr("Audio and display"), tr("Core listener IP"), tr("Radio")}) {
        auto* label = text(caption, connection, true);
        label->setFixedWidth(142);
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        facts->addWidget(label, row, 0, Qt::AlignTop);
        auto* value = text({}, connection);
        m_facts.append(value);
        facts->addWidget(value, row++, 1, Qt::AlignTop);
    }
    facts->setColumnStretch(1, 1);
    qobject_cast<QVBoxLayout*>(connection->layout())->addLayout(facts);
    overviewLayout->addWidget(connection);
    overviewLayout->addWidget(text(tr("Controls and audio/display can use different paths. A saved address is not proof of the current path."), overview, true));
    auto* links = new QHBoxLayout;
    auto* manage = button(tr("Manage addresses…"), overview, "manageCoreAddresses");
    links->addWidget(manage);
    m_audio = button(tr("Audio with the Core…"), overview);
    m_audio->setToolTip(tr("Audio for this window’s current Core connection."));
    links->addWidget(m_audio);
    links->addStretch();
    overviewLayout->addLayout(links);
    auto* destinations = new QHBoxLayout;
    m_details = button(tr("Connection details…"), overview);
    m_diagnostics = button(tr("Diagnostics…"), overview);
    m_details->setToolTip(tr("Connection details for this window."));
    m_diagnostics->setToolTip(tr("Network diagnostics for this window’s current connection."));
    destinations->addWidget(m_details);
    destinations->addWidget(m_diagnostics);
    destinations->addStretch();
    overviewLayout->addLayout(destinations);
    overviewLayout->addStretch();
    connect(manage, &QPushButton::clicked, this, [this] { m_tabs->setCurrentIndex(1); });
    connect(m_audio, &QPushButton::clicked, this, &CoresSetupPage::audioRequested);
    connect(m_details, &QPushButton::clicked, this, &CoresSetupPage::connectionDetailsRequested);
    connect(m_diagnostics, &QPushButton::clicked, this, &CoresSetupPage::diagnosticsRequested);
    connect(m_rename, &QPushButton::clicked, this, &CoresSetupPage::openRenameEditor);
    connect(cancelName, &QPushButton::clicked, this, &CoresSetupPage::closeRenameEditor);
    connect(m_nameSave, &QPushButton::clicked, this, [this] {
        if (!m_store || !renameTarget() || m_renameOperation != 0) { return; }
        const std::optional<StationLabel> name = StationLabel::parse(m_nameInput->text());
        if (!name) {
            m_nameError->setText(tr("Enter a callsign and an optional /name using the characters described above."));
            m_nameInput->setFocus();
            return;
        }
        m_nameError->clear();
        m_renameOperation = ++m_nextRenameOperation;
        m_nameInput->setEnabled(false);
        m_nameSave->setEnabled(false);
        m_nameProgress->setText(tr("Waiting for the Core to accept its new name…"));
        m_renameRequest = CoreRenameRequest{m_renameOperation, m_inspectedId,
            m_context.renamePairedIdentity, m_incarnation, m_context.renameEpoch, name->display()};
        emit renameRequested(*m_renameRequest);
    });
    connect(m_nameInput, &QLineEdit::returnPressed, m_nameSave, &QPushButton::click);

    auto* addresses = new QWidget(m_panels);
    auto* addressLayout = vertical(addresses);
    m_panels->addWidget(addresses);
    m_addressList = new QWidget(addresses);
    auto* listLayout = vertical(m_addressList);
    listLayout->addWidget(text(tr("This computer tries available addresses automatically when connecting. Reachable direct addresses can still work when the remote access service is unavailable."), m_addressList));
    for (const QString& title : {tr("Added by you"), tr("Previously worked"), tr("Supplied by the Core")}) {
        auto* box = group(title, m_addressList);
        auto* rows = new QWidget(box);
        vertical(rows);
        qobject_cast<QVBoxLayout*>(box->layout())->addWidget(rows);
        m_addressGroups.append(rows);
        listLayout->addWidget(box);
        if (m_addressGroups.size() == 1) {
            auto* addRow = new QHBoxLayout;
            m_addAddress = button(tr("Add address…"), box, "addCoreAddress");
            m_addressCount = text({}, box, true);
            addRow->addWidget(m_addAddress);
            addRow->addWidget(m_addressCount, 1);
            qobject_cast<QVBoxLayout*>(box->layout())->addLayout(addRow);
        }
    }
    listLayout->addWidget(text(tr("Address changes apply when connecting. They do not switch the current connection. Successful addresses are remembered separately, up to four."), m_addressList, true));
    addressLayout->addWidget(m_addressList);
    m_addressEditor = group({}, addresses);
    m_addressEditor->setObjectName(QStringLiteral("coreAddressEditor"));
    auto* form = qobject_cast<QVBoxLayout*>(m_addressEditor->layout());
    form->addWidget(text(tr("The address must identify this saved Core. This does not connect the window or pair another Core."), m_addressEditor));
    auto* inputRow = new QHBoxLayout;
    auto* hostColumn = new QVBoxLayout;
    auto* hostLabel = text(tr("Hostname or IP address"), m_addressEditor);
    m_host = new QLineEdit(m_addressEditor);
    m_host->setObjectName(QStringLiteral("coreAddressHost"));
    m_host->setPlaceholderText(QStringLiteral("radxa.example"));
    m_host->setAccessibleName(tr("Hostname or IP address"));
    hostLabel->setBuddy(m_host);
    hostColumn->addWidget(hostLabel);
    hostColumn->addWidget(m_host);
    inputRow->addLayout(hostColumn, 1);
    auto* portColumn = new QVBoxLayout;
    auto* portLabel = text(tr("Core connection port"), m_addressEditor);
    m_port = new QSpinBox(m_addressEditor);
    m_port->setObjectName(QStringLiteral("coreAddressPort"));
    m_port->setRange(1, 65535);
    m_port->setValue(41000);
    m_port->setAccessibleName(tr("Core connection port"));
    portLabel->setBuddy(m_port);
    portColumn->addWidget(portLabel);
    portColumn->addWidget(m_port);
    inputRow->addLayout(portColumn);
    form->addLayout(inputRow);
    form->addWidget(text(tr("IPv4, hostname, or IPv6 (with or without brackets). This is the Core listener port."), m_addressEditor, true));
    auto* formButtons = new QHBoxLayout;
    m_submitAddress = button(tr("Add"), m_addressEditor, "submitCoreAddress");
    auto* cancelAddress = button(tr("Cancel"), m_addressEditor, "cancelCoreAddress");
    formButtons->addWidget(m_submitAddress);
    formButtons->addWidget(cancelAddress);
    formButtons->addStretch();
    form->addLayout(formButtons);
    m_addressError = text({}, m_addressEditor);
    m_addressError->setObjectName(QStringLiteral("coreAddressError"));
    m_addressError->setStyleSheet(QStringLiteral("color:#ffa3a3;"));
    form->addWidget(m_addressError);
    m_addressProgress = text({}, m_addressEditor);
    m_addressProgress->setObjectName(QStringLiteral("coreAddressProgress"));
    form->addWidget(m_addressProgress);
    addressLayout->addWidget(m_addressEditor);
    m_addressEditor->hide();
    m_removeConfirmation = group(tr("Remove manual retention?"), addresses);
    m_removeText = text({}, m_removeConfirmation);
    auto* removeLayout = qobject_cast<QVBoxLayout*>(m_removeConfirmation->layout());
    removeLayout->addWidget(m_removeText);
    auto* removeButtons = new QHBoxLayout;
    auto* confirmRemove = button(tr("Remove"), m_removeConfirmation, "confirmRemoveCoreAddress");
    auto* cancelRemove = button(tr("Cancel"), m_removeConfirmation);
    removeButtons->addWidget(confirmRemove);
    removeButtons->addWidget(cancelRemove);
    removeButtons->addStretch();
    removeLayout->addLayout(removeButtons);
    addressLayout->addWidget(m_removeConfirmation);
    m_removeConfirmation->hide();
    addressLayout->addStretch();
    connect(m_addAddress, &QPushButton::clicked, this, [this] { openAddressEditor(); });
    connect(cancelAddress, &QPushButton::clicked, this, &CoresSetupPage::closeAddressEditor);
    connect(m_submitAddress, &QPushButton::clicked, this, &CoresSetupPage::submitAddress);
    connect(m_host, &QLineEdit::returnPressed, m_submitAddress, &QPushButton::click);
    connect(cancelRemove, &QPushButton::clicked, m_removeConfirmation, &QWidget::hide);
    connect(confirmRemove, &QPushButton::clicked, this, [this] {
        QString error;
        if (m_addresses && m_addresses->removeAddress(m_removingAddress, &error)) {
            m_removeConfirmation->hide();
            m_status->setText(tr("Manual retention removed. Automatic address sources and this window’s connection are unchanged."));
            renderAddresses();
        } else {
            m_status->setText(error);
        }
    });
    m_radioPanel = new QWidget(m_panels);
    vertical(m_radioPanel);
    m_panels->addWidget(m_radioPanel);
    m_devicesPanel = new QWidget(m_panels);
    vertical(m_devicesPanel);
    m_panels->addWidget(m_devicesPanel);
    m_status = text({}, this);
    m_status->setObjectName(QStringLiteral("coreSettingsStatus"));
    layout->addWidget(m_status);
    layout->addStretch();
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        cancelOperations();
        m_panels->setCurrentIndex(index);
        for (int i = 0; i < m_panels->count(); ++i) {
            m_panels->widget(i)->setSizePolicy(QSizePolicy::Ignored,
                i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        }
        m_panels->updateGeometry();
    });
    for (int i = 1; i < m_panels->count(); ++i) {
        m_panels->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
    if (m_store) {
        m_addresses = new CoreAddressController(*m_store, this, std::move(factory), deadlineMs);
        connect(m_addresses, &CoreAddressController::finished, this,
                [this](quint64 id, CoreAddressController::Outcome outcome, const QString& reason) {
            if (id == 0 || id != m_addressOperation || !m_store) { return; }
            if (m_store->targetIncarnation(m_inspectedId) != m_incarnation) {
                cancelOperations();
                refreshTargets();
                return;
            }
            m_addressOperation = 0;
            m_host->setEnabled(true);
            m_port->setEnabled(true);
            m_submitAddress->setEnabled(true);
            m_addressProgress->clear();
            if (outcome == CoreAddressController::Outcome::Saved) {
                closeAddressEditor();
                renderAddresses();
                m_status->setText(tr("Address saved for future connections. It has not been marked as previously worked."));
            } else if (outcome != CoreAddressController::Outcome::Cancelled) {
                const bool genericRefusal = outcome == CoreAddressController::Outcome::Refused
                    && reason == QStringLiteral("The Core could not be reached from here.");
                m_addressError->setText(genericRefusal
                    ? tr("This address could not be verified as this saved Core. It was not saved.") : reason);
                m_submitAddress->setText(tr("Retry"));
                m_host->setFocus();
            }
        });
    }
    refreshTargets();
}

CoresSetupPage::~CoresSetupPage()
{
    cancelOperations();
}

bool CoresSetupPage::renameTarget() const
{
    const auto target = m_store ? m_store->target(m_inspectedId) : std::nullopt;
    return target && m_context.renameAvailable && m_context.renameEpoch != 0
        && m_context.renameTargetId == m_inspectedId
        && m_context.renameIncarnation == m_store->targetIncarnation(m_inspectedId)
        && !target->connection.allowUnpinned && m_context.renamePairedIdentity.size() == 32
        && target->connection.identityFingerprint == m_context.renamePairedIdentity;
}

bool CoresSetupPage::currentTarget() const
{
    const auto target = m_store ? m_store->target(m_inspectedId) : std::nullopt;
    return target && m_context.authenticated && m_context.epoch != 0
        && m_context.targetId == m_inspectedId && m_context.pairedIdentity.size() == 32
        && !target->connection.allowUnpinned
        && target->connection.identityFingerprint == m_context.pairedIdentity;
}
QString CoresSetupPage::coreName(const SavedCoreTarget& target) const
{
    if (m_context.authenticated && m_context.epoch != 0 && target.id == m_context.targetId
        && !target.connection.allowUnpinned && target.connection.identityFingerprint.size() == 32
        && target.connection.identityFingerprint == m_context.pairedIdentity) {
        const QString name = m_radioModel && m_radioModel->stationDevices()
            ? m_radioModel->stationDevices()->coreInfo().stationLabel : m_context.coreName;
        return name.isEmpty() ? tr("Core name unknown") : name;
    }
    if (target.lastKnownCoreName && !target.connection.allowUnpinned
        && target.lastKnownCoreName->pairedIdentity == target.connection.identityFingerprint) {
        return tr("%1 (last known)").arg(target.lastKnownCoreName->name);
    }
    return tr("Core name unknown");
}
void CoresSetupPage::setContext(const CoreSettingsContext& context)
{
    if (m_context.epoch != context.epoch || m_context.targetId != context.targetId
        || m_context.pairedIdentity != context.pairedIdentity
        || m_context.authenticated != context.authenticated
        || m_context.renameTargetId != context.renameTargetId
        || m_context.renamePairedIdentity != context.renamePairedIdentity
        || m_context.renameIncarnation != context.renameIncarnation
        || m_context.renameEpoch != context.renameEpoch) {
        cancelOperations();
    } else if (m_context.renameAvailable && !context.renameAvailable) {
        // Capability/active-session authority can be revoked without a new
        // epoch. Retire rename only; address verification has separate authority.
        closeRenameEditor();
    }
    m_context = context;
    refreshTargets();
}
void CoresSetupPage::refreshTargets()
{
    const bool restoreCoreFocus = m_coreList->isAncestorOf(QApplication::focusWidget());
    clearLayout(m_coreGrid);
    const QList<SavedCoreTarget> targets = m_store ? m_store->targets() : QList<SavedCoreTarget>{};
    if (!m_store || targets.isEmpty()) {
        m_coreGrid->addWidget(text(m_store ? tr("No saved Cores. Add a Core in Connections.")
                                         : tr("Saved Cores are unavailable in this window."), m_coreList, true), 0, 0);
        inspectTarget({});
        return;
    }
    if (!m_store->target(m_inspectedId)) {
        m_inspectedId = targets.first().id;
    }
    QPointer<QPushButton> selectedEntry;
    QStringList unknownIdentifiers;
    for (const SavedCoreTarget& target : targets) {
        if (coreName(target) == tr("Core name unknown")) {
            unknownIdentifiers.append(savedIdentifier(target));
        }
    }
    int position = 0;
    for (const SavedCoreTarget& target : targets) {
        auto* entry = new CoreChoiceButton(m_coreList);
        entry->setProperty("coreTargetId", target.id);
        const QString name = coreName(target);
        auto* entryLayout = new QVBoxLayout(entry);
        entryLayout->setContentsMargins(8, 4, 8, 4);
        entryLayout->setSpacing(0);
        entryLayout->addWidget(text(name, entry));
        const bool connected = m_context.authenticated && m_context.epoch != 0
            && target.id == m_context.targetId && !target.connection.allowUnpinned
            && target.connection.identityFingerprint.size() == 32
            && target.connection.identityFingerprint == m_context.pairedIdentity;
        QString description = connected ? tr("Connected in this window") : tr("Not connected in this window");
        if (name == tr("Core name unknown")) {
            QString identifier = savedIdentifier(target);
            // Shared configured endpoints/IDs need a stable discriminator too.
            if (unknownIdentifiers.count(identifier) > 1) {
                identifier += tr(" · Core identity: %1").arg(target.connection.identityFingerprint.isEmpty()
                    ? target.id : QString::fromLatin1(target.connection.identityFingerprint.toHex().left(12)));
            }
            description += '\n' + identifier;
        }
        entry->setAccessibleName(name + '\n' + description);
        QString details = name + '\n' + description;
        if (name == tr("Core name unknown") && !target.connection.identityFingerprint.isEmpty()) {
            details += '\n' + tr("Core identity: %1").arg(QString::fromLatin1(target.connection.identityFingerprint.toHex()));
        }
        entry->setAccessibleDescription(details);
        entry->setToolTip(details);
        auto* subtitle = text(description, entry);
        subtitle->setStyleSheet(QStringLiteral("font-size:11px; background:transparent;"));
        entryLayout->addWidget(subtitle);
        if (target.id == m_inspectedId) {
            selectedEntry = entry;
            entry->setStyleSheet(QStringLiteral("QPushButton { background:#00b4d8; border:0; border-radius:0; padding:0; } QLabel { color:#0f0f1a; }"));
            for (QLabel* label : entry->findChildren<QLabel*>()) {
                label->setStyleSheet(QStringLiteral("color:#0f0f1a; background:transparent; font-size:%1px;").arg(label == subtitle ? 11 : 12));
            }
        } else {
            entry->setStyleSheet(QStringLiteral("QPushButton { background:#1a2a3a; border:0; border-radius:0; padding:0; }"));
        }
        connect(entry, &QPushButton::clicked, this, [this, id = target.id] { inspectTarget(id); refreshTargets(); });
        m_coreGrid->addWidget(entry, position / 2, position % 2);
        ++position;
    }
    m_coreGrid->setColumnStretch(0, 1);
    m_coreGrid->setColumnStretch(1, 1);
    inspectTarget(m_inspectedId);
    if (restoreCoreFocus && selectedEntry) { selectedEntry->setFocus(); }
}
void CoresSetupPage::inspectTarget(const QString& id)
{
    const quint64 incarnation = m_store ? m_store->targetIncarnation(id) : 0;
    if (id != m_inspectedId || incarnation != m_incarnation) {
        cancelOperations();
        m_status->clear();
    }
    m_inspectedId = id;
    m_incarnation = incarnation;
    if (m_addresses) { m_addresses->inspectTarget(id); }
    renderInspection();
    renderAddresses();
    renderAdministration();
}
void CoresSetupPage::renderInspection()
{
    const auto target = m_store ? m_store->target(m_inspectedId) : std::nullopt;
    m_tabs->setEnabled(target.has_value());
    m_panels->setVisible(target.has_value());
    const QString name = target ? coreName(*target) : QString();
    m_heading->setText(name);
    m_name->setText(name);
    m_rename->setEnabled(renameTarget());
    QString renameReason = m_context.renameTargetId == m_inspectedId ? m_context.renameReason : QString();
    if (!target || target->connection.identityFingerprint.size() != 32 || target->connection.allowUnpinned) {
        m_rename->setEnabled(false);
        renameReason = tr("Pair with this Core again first.");
    } else if (!renameTarget() && renameReason.isEmpty()) {
        renameReason = currentTarget() ? tr("Rename is unavailable from this Core connection.")
                                       : tr("Connect to this Core to rename it.");
    }
    m_nameReason->setText(renameReason);
    const bool current = currentTarget();
    const QString unavailable = tr("Unavailable");
    const QString unknown = tr("Not reported for this connection");
    const QStringList values = {current ? tr("Connected") : tr("Not connected in this window"),
        current ? (m_context.reachedThrough.isEmpty() ? unknown : m_context.reachedThrough) : unavailable,
        current ? (m_context.controls.isEmpty() ? unknown : m_context.controls) : unavailable,
        current ? (m_context.audioAndDisplay.isEmpty() ? unknown : m_context.audioAndDisplay) : unavailable,
        current ? (m_context.listener.isEmpty() ? unknown : m_context.listener) : unavailable,
        current ? (m_context.radio.isEmpty() ? tr("Not reported by the Core") : m_context.radio)
                : tr("Not available from a current connection")};
    for (int i = 0; i < values.size(); ++i) { m_facts[i]->setText(values[i]); }
    m_facts[0]->setStyleSheet(current ? "color:#00d286;" : "color:#8090a0;");
    // These destinations always concern this window, even when inspecting another Core.
    m_audio->setEnabled(m_context.audioAvailable);
    m_details->setEnabled(m_context.connectionDetailsAvailable);
    m_diagnostics->setEnabled(m_context.diagnosticsAvailable);
}
void CoresSetupPage::renderAddresses()
{
    const auto target = m_store ? m_store->target(m_inspectedId) : std::nullopt;
    for (QWidget* rows : m_addressGroups) { clearLayout(rows->layout()); }
    const QStringList manual = target ? target->manualAddresses : QStringList{};
    const QStringList worked = target ? target->connection.cachedAddresses : QStringList{};
    const QStringList supplied = target ? target->connection.coreAddresses : QStringList{};
    const QList<QStringList> sources = {manual, worked, supplied};
    QStringList shown;
    for (int source = 0; source < sources.size(); ++source) {
        QWidget* rows = m_addressGroups[source];
        int count = 0;
        for (const QString& address : sources[source]) {
            if (shown.contains(address)) { continue; }
            shown.append(address);
            ++count;
            auto* row = new QWidget(rows);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 6, 0, 6);
            rowLayout->setSpacing(8);
            auto* info = new QWidget(row);
            auto* infoLayout = vertical(info);
            infoLayout->setSpacing(0);
            infoLayout->addWidget(text(addressDisplay(address), info));
            QStringList evidence;
            if (manual.contains(address)) { evidence.append(tr("Added by you")); }
            if (worked.contains(address)) { evidence.append(tr("Previously worked")); }
            if (supplied.contains(address)) { evidence.append(tr("Supplied by Core · may not be reachable")); }
            auto* notes = text(evidence.join(QStringLiteral(" · ")), info, true);
            notes->setStyleSheet(QStringLiteral("font-size:11px; color:#8090a0;"));
            infoLayout->addWidget(notes);
            rowLayout->addWidget(info, 1);
            if (manual.contains(address)) {
                auto* edit = button(tr("Edit…"), row, "editCoreAddress");
                auto* remove = button(tr("Remove"), row, "removeCoreAddress");
                rowLayout->addWidget(edit);
                rowLayout->addWidget(remove);
                connect(edit, &QPushButton::clicked, this, [this, address] { openAddressEditor(address); });
                connect(remove, &QPushButton::clicked, this, [this, address] { removeAddress(address); });
            }
            rows->layout()->addWidget(row);
        }
        if (count == 0) {
            const QStringList empty = {tr("No manually retained addresses."),
                worked.isEmpty() ? tr("No successful direct addresses saved.") : tr("Previously worked addresses are marked above."),
                supplied.isEmpty() ? tr("No address inventory reported by this Core.") : tr("Core-supplied addresses are marked above. They may not be reachable now.")};
            rows->layout()->addWidget(text(empty[source], rows, true));
        }
    }
    m_addressCount->setText(tr("%1 of 4").arg(manual.size()));
    const bool paired = target && !target->connection.allowUnpinned && target->connection.identityFingerprint.size() == 32;
    m_addAddress->setEnabled(paired && manual.size() < CoreTargetStore::kMaxManualAddresses);
    m_addAddress->setToolTip(!paired ? tr("Pair with this Core again first.")
        : manual.size() >= CoreTargetStore::kMaxManualAddresses ? tr("Four addresses are retained. Edit or remove one to add another.") : QString());
}
void CoresSetupPage::renderAdministration()
{
    const bool current = currentTarget() && m_radioModel;
    if (m_administration && current && m_adminTarget == m_inspectedId && m_adminEpoch == m_context.epoch) {
        m_administration->setStationSettingsAvailable(m_dialogStationAvailable && m_context.stationSettingsAvailable,
            !m_dialogStationAvailable ? m_dialogStationReason : m_context.stationSettingsReason);
        return;
    }
    for (const auto& section : m_adminSections) {
        if (section) { section->hide(); section->deleteLater(); }
    }
    m_adminSections.clear();
    if (m_administration) { m_administration->deleteLater(); m_administration = nullptr; }
    clearLayout(m_radioPanel->layout());
    clearLayout(m_devicesPanel->layout());
    if (current) {
        m_administration = new ThisCorePage(m_radioModel, this);
        m_administration->hide();
        m_administration->setStationSettingsAvailable(m_dialogStationAvailable && m_context.stationSettingsAvailable,
            !m_dialogStationAvailable ? m_dialogStationReason : m_context.stationSettingsReason);
        QWidget* radio = m_administration->radioSection();
        radio->setParent(m_radioPanel);
        m_radioPanel->layout()->addWidget(radio);
        radio->show();
        m_adminSections.append(radio);
        for (QWidget* section : m_administration->devicesSections()) {
            section->setParent(m_devicesPanel);
            m_devicesPanel->layout()->addWidget(section);
            section->show();
            m_adminSections.append(section);
        }
        m_adminTarget = m_inspectedId;
        m_adminEpoch = m_context.epoch;
    } else {
        auto* radio = group(tr("Change radio"), m_radioPanel);
        auto* radioLayout = qobject_cast<QVBoxLayout*>(radio->layout());
        radioLayout->addWidget(text(tr("The Core runs one radio at a time. Changing it restarts its radio connection; every window and device reconnects."), radio));
        const QString radioReason = tr("Connect this window to this Core to change its radio.");
        radioLayout->addWidget(text(radioReason, radio, true));
        radioLayout->addWidget(text(tr("Current radio: Not available from a current connection"), radio));
        radioLayout->addWidget(text(tr("Radio network address: Unavailable"), radio, true));
        auto* unavailableRadioActions = new QHBoxLayout;
        for (const QString& caption : {tr("Use this radio"), tr("Scan again"), tr("Forget radio")}) {
            auto* action = button(caption, radio);
            action->setEnabled(false);
            action->setToolTip(radioReason);
            unavailableRadioActions->addWidget(action);
        }
        unavailableRadioActions->addStretch();
        radioLayout->addLayout(unavailableRadioActions);
        m_radioPanel->layout()->addWidget(radio);
        const QString devicesReason = tr("Connect this window to this Core to manage its devices.");
        for (const QString& title : {tr("Connected now"), tr("Paired devices"), tr("Core identity")}) {
            auto* section = group(title, m_devicesPanel);
            auto* sectionLayout = qobject_cast<QVBoxLayout*>(section->layout());
            sectionLayout->addWidget(text(devicesReason, section, true));
            if (title == tr("Paired devices")) {
                auto* action = button(tr("Add a device"), section);
                action->setEnabled(false);
                action->setToolTip(devicesReason);
                sectionLayout->addWidget(action, 0, Qt::AlignLeft);
            } else if (title == tr("Core identity")) {
                sectionLayout->addWidget(text(m_heading->text(), section));
                sectionLayout->addWidget(text(tr("Key-backup status is unavailable without a current connection."), section, true));
                auto* action = button(tr("I've backed it up"), section);
                action->setEnabled(false);
                action->setToolTip(devicesReason);
                sectionLayout->addWidget(action, 0, Qt::AlignLeft);
            }
            m_devicesPanel->layout()->addWidget(section);
        }
    }
    qobject_cast<QVBoxLayout*>(m_radioPanel->layout())->addStretch();
    qobject_cast<QVBoxLayout*>(m_devicesPanel->layout())->addStretch();
}
void CoresSetupPage::openAddressEditor(const QString& previous)
{
    if (!m_addresses) { return; }
    closeAddressEditor();
    m_status->clear();
    m_previousAddress = previous;
    m_addressEditor->setTitle(previous.isEmpty() ? tr("Add an address for %1").arg(m_heading->text())
                                                : tr("Edit an address for %1").arg(m_heading->text()));
    m_submitAddress->setText(previous.isEmpty() ? tr("Add") : tr("Save"));
    m_host->setText(previous.isEmpty() ? QString() : QUrl(previous).host());
    m_port->setValue(previous.isEmpty() ? 41000 : QUrl(previous).port(41000));
    m_addressList->hide();
    m_removeConfirmation->hide();
    m_addressEditor->show();
    m_host->setFocus();
}
void CoresSetupPage::closeAddressEditor()
{
    m_addressOperation = 0;
    if (m_addresses) { m_addresses->cancel(); }
    m_addressEditor->hide();
    m_addressList->show();
    m_addressError->clear();
    m_addressProgress->clear();
    m_host->setEnabled(true);
    m_port->setEnabled(true);
    m_submitAddress->setEnabled(true);
    m_previousAddress.clear();
}
void CoresSetupPage::submitAddress()
{
    if (!m_addresses || m_addressOperation != 0) { return; }
    m_addressError->clear();
    m_addressOperation = m_previousAddress.isEmpty()
        ? m_addresses->addAddress(m_host->text(), m_port->value())
        : m_addresses->editAddress(m_previousAddress, m_host->text(), m_port->value());
    m_host->setEnabled(false);
    m_port->setEnabled(false);
    m_submitAddress->setEnabled(false);
    m_addressProgress->setText(tr("Checking that this address reaches this saved Core…"));
}
void CoresSetupPage::removeAddress(const QString& address)
{
    const auto target = m_store ? m_store->target(m_inspectedId) : std::nullopt;
    if (!target) { return; }
    m_removingAddress = address;
    const bool automatic = target->connection.cachedAddresses.contains(address)
        || target->connection.coreAddresses.contains(address);
    m_removeText->setText(tr("%1 — remove the entry added by you. %2").arg(addressDisplay(address),
        automatic ? tr("It remains an automatic candidate because it previously worked or is supplied by the Core.")
                  : tr("This does not disconnect the window. It may return if a later session succeeds or the Core supplies it.")));
    m_removeConfirmation->show();
}
void CoresSetupPage::openRenameEditor()
{
    if (!m_rename->isEnabled()) { return; }
    m_status->clear();
    const auto target = m_store->target(m_inspectedId);
    m_nameInput->setText(currentTarget() ? (m_radioModel && m_radioModel->stationDevices()
        ? m_radioModel->stationDevices()->coreInfo().stationLabel : m_context.coreName)
        : target && target->lastKnownCoreName ? target->lastKnownCoreName->name : QString());
    m_nameDisplay->hide();
    m_nameEditor->show();
    m_nameInput->setFocus();
}
void CoresSetupPage::closeRenameEditor()
{
    const quint64 id = m_renameOperation;
    m_renameOperation = 0;
    m_renameRequest.reset();
    m_nameEditor->hide();
    m_nameDisplay->show();
    m_nameInput->setEnabled(true);
    m_nameSave->setEnabled(true);
    m_nameProgress->clear();
    m_nameError->clear();
    if (id != 0) { emit renameCancelled(id); }
}
void CoresSetupPage::finishRename(const CoreRenameRequest& request, bool accepted, const QString& reason)
{
    if (!m_store || !m_renameRequest || request != *m_renameRequest
        || request.operationId == 0 || request.operationId != m_renameOperation
        || request.targetId != m_inspectedId || request.epoch != m_context.renameEpoch
        || request.pairedIdentity != m_context.renamePairedIdentity || !renameTarget()
        || request.incarnation != m_incarnation
        || m_store->targetIncarnation(request.targetId) != request.incarnation) { return; }
    m_renameOperation = 0;
    m_renameRequest.reset();
    m_nameProgress->clear();
    m_nameInput->setEnabled(true);
    m_nameSave->setEnabled(true);
    if (accepted) {
        closeRenameEditor();
        refreshTargets();
        m_status->setText(reason.isEmpty()
            ? tr("The Core accepted its new name. Core identity and address history are unchanged.") : reason);
    } else {
        m_nameError->setText(reason);
        m_nameInput->setFocus();
    }
}
void CoresSetupPage::cancelOperations()
{
    closeAddressEditor();
    closeRenameEditor();
    m_removeConfirmation->hide();
    m_removingAddress.clear();
}
void CoresSetupPage::hideEvent(QHideEvent* event)
{
    cancelOperations();
    SetupPage::hideEvent(event);
}
void CoresSetupPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    // The host's identity-specific snapshot remains authoritative. A dialog's
    // generic gate can further restrict, never admit an unmatched inspected Core.
    m_dialogStationAvailable = available;
    m_dialogStationReason = reason;
    if (m_administration) {
        m_administration->setStationSettingsAvailable(available && m_context.stationSettingsAvailable,
            !available ? reason : m_context.stationSettingsReason);
    }
}
} // namespace NereusSDR
