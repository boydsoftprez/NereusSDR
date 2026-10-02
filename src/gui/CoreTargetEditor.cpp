// =================================================================
// src/gui/CoreTargetEditor.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
// =================================================================

// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#include "gui/CoreTargetEditor.h"

#include "core/session/RemoteStationOptions.h"

#include <QCheckBox>
#include <QDateTime>
#include <QTimer>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <utility>

namespace NereusSDR {
namespace {

constexpr int kLabelMaximumLength = 512;
constexpr int kUrlMaximumLength = 4096;
constexpr int kTokenMaximumLength = 8192;
constexpr int kFingerprintMaximumLength = 256;

} // namespace

CoreTargetEditor::CoreTargetEditor(const SavedCoreTarget& initial, QWidget* parent)
    : QDialog(parent)
    , m_initial(initial)
{
    const bool serviceOnly = initial.connection.url.isEmpty()
        && !initial.connection.identityFingerprint.isEmpty();
    setWindowTitle(tr("Core setup"));
    setObjectName(QStringLiteral("coreTargetEditor"));

    auto* layout = new QVBoxLayout(this);
    auto* explanation = new QLabel(serviceOnly
        ? tr("This paired Core uses remote access. You can change its name and connection preference here.")
        : tr("Enter the Core address and its pairing token. The certificate fingerprint must come from Core setup."),
        this);
    explanation->setObjectName(QStringLiteral("coreTargetEditorExplanation"));
    explanation->setTextFormat(Qt::PlainText);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* form = new QFormLayout();
    m_labelEdit = new QLineEdit(initial.label, this);
    m_labelEdit->setObjectName(QStringLiteral("coreTargetEditorLabel"));
    m_labelEdit->setMaxLength(kLabelMaximumLength);
    m_addressEdit = new QLineEdit(initial.connection.url, this);
    m_addressEdit->setObjectName(QStringLiteral("coreTargetEditorAddress"));
    m_addressEdit->setMaxLength(kUrlMaximumLength);
    m_tokenEdit = new QLineEdit(initial.connection.token, this);
    m_tokenEdit->setObjectName(QStringLiteral("coreTargetEditorToken"));
    m_tokenEdit->setMaxLength(kTokenMaximumLength);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_fingerprintEdit = new QLineEdit(initial.connection.fingerprint, this);
    m_fingerprintEdit->setObjectName(QStringLiteral("coreTargetEditorFingerprint"));
    m_fingerprintEdit->setMaxLength(kFingerprintMaximumLength);
    m_allowUnpinnedCheck = new QCheckBox(tr("Connect without a certificate fingerprint (bench only)"), this);
    m_allowUnpinnedCheck->setObjectName(QStringLiteral("coreTargetEditorAllowUnpinned"));
    m_allowUnpinnedCheck->setChecked(initial.connection.allowUnpinned);
    form->addRow(tr("Label:"), m_labelEdit);
    form->addRow(tr("Address:"), m_addressEdit);
    form->addRow(tr("Token:"), m_tokenEdit);
    form->addRow(tr("Certificate fingerprint:"), m_fingerprintEdit);
    form->addRow({}, m_allowUnpinnedCheck);
    if (serviceOnly) {
        form->setRowVisible(m_addressEdit, false);
        form->setRowVisible(m_tokenEdit, false);
        form->setRowVisible(m_fingerprintEdit, false);
        form->setRowVisible(m_allowUnpinnedCheck, false);
    }
    // iPhone app plan Task 29 (R-IOS-16): a paired Core is also reached
    // through the internet service, raced with its addresses; the operator
    // may turn that off for this Core. Shown for every Core, disabled with
    // the reason where it cannot run.
    m_reachAnywhereCheck = new QCheckBox(
        tr("Also reach this Core from anywhere through the internet service"), this);
    m_reachAnywhereCheck->setObjectName(QStringLiteral("coreTargetEditorReachAnywhere"));
    m_reachAnywhereCheck->setChecked(initial.connection.reachFromAnywhere);
    const QString refusal = initial.connection.serviceConnectRefusal();
    m_reachAnywhereCheck->setEnabled(refusal.isEmpty());
    form->addRow({}, m_reachAnywhereCheck);
    m_reachAnywhereReason = new QLabel(refusal, this);
    m_reachAnywhereReason->setObjectName(QStringLiteral("coreTargetEditorReachAnywhereReason"));
    m_reachAnywhereReason->setTextFormat(Qt::PlainText);
    m_reachAnywhereReason->setWordWrap(true);
    m_reachAnywhereReason->setVisible(!refusal.isEmpty());
    form->addRow({}, m_reachAnywhereReason);
    if (initial.connection.effectiveControlChannelVersion(
            QDateTime::currentMSecsSinceEpoch()) == 0) {
        const qint64 remaining = initial.connection.negativeControlObservedMs
            + RemoteStationOptions::kNegativeControlLifetimeMs
            - QDateTime::currentMSecsSinceEpoch();
        QTimer::singleShot(static_cast<int>(remaining > 0 ? remaining : 1), this,
                           &CoreTargetEditor::refreshServiceAvailability);
    }
    layout->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("coreTargetEditorError"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);
    layout->addWidget(m_errorLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto* saveButton = buttons->button(QDialogButtonBox::Save);
    auto* cancelButton = buttons->button(QDialogButtonBox::Cancel);
    saveButton->setObjectName(QStringLiteral("coreTargetEditorSave"));
    cancelButton->setObjectName(QStringLiteral("coreTargetEditorCancel"));
    saveButton->setAutoDefault(false);
    cancelButton->setAutoDefault(false);
    layout->addWidget(buttons);
    connect(saveButton, &QPushButton::clicked, this, [this] {
        if (validate()) {
            accept();
        }
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    resize(540, sizeHint().height());
}

void CoreTargetEditor::setCurrentOptionsSource(CurrentOptionsSource source)
{
    m_currentOptionsSource = std::move(source);
    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &CoreTargetEditor::refreshServiceAvailability);
    timer->start();
    refreshServiceAvailability();
}

void CoreTargetEditor::refreshServiceAvailability()
{
    if (m_currentOptionsSource) {
        if (const auto current = m_currentOptionsSource();
            current && current->identityFingerprint == m_initial.connection.identityFingerprint) {
            m_initial.connection.controlChannelVersion = current->controlChannelVersion;
            m_initial.connection.negativeControlObservedMs = current->negativeControlObservedMs;
        }
    }
    const QString refusal = m_initial.connection.serviceConnectRefusal();
    m_reachAnywhereCheck->setEnabled(refusal.isEmpty());
    m_reachAnywhereReason->setText(refusal);
    m_reachAnywhereReason->setVisible(!refusal.isEmpty());
}

SavedCoreTarget CoreTargetEditor::target() const
{
    SavedCoreTarget result = m_initial;
    result.label = m_labelEdit->text().trimmed();
    result.connection.url = m_addressEdit->text().trimmed();
    result.connection.token = m_tokenEdit->text();
    result.connection.fingerprint = m_fingerprintEdit->text();
    result.connection.allowUnpinned = m_allowUnpinnedCheck->isChecked();
    result.connection.reachFromAnywhere = m_reachAnywhereCheck->isChecked();
    if (result.connection.url != m_initial.connection.url) {
        result.lastRadioName.clear();
        result.lastRadioMac.clear();
    }
    // iPhone app plan Task 27 fix wave (I1): the Core's last good addresses
    // were reached with the old address, token, pin and identity. With any
    // of those changed they may name another computer, and the next connect
    // would try them first with the new token, so they are forgotten.
    const RemoteStationOptions& before = m_initial.connection;
    const RemoteStationOptions& after = result.connection;
    if (after.url != before.url || after.token != before.token
        || after.fingerprint != before.fingerprint || after.allowUnpinned != before.allowUnpinned
        || after.identityFingerprint != before.identityFingerprint) {
        result.connection.cachedAddresses.clear();
        result.connection.coreAddresses.clear();
        // Task 28 fix wave: what the old Core declared says nothing of the
        // new one.
        result.connection.controlChannelVersion = -1;
        result.connection.negativeControlObservedMs = -1;
    }
    // Task 29: another identity is another Core: where the service finds
    // it, and its relay setting, were the old one's.
    if (after.identityFingerprint != before.identityFingerprint) {
        result.connection.rendezvousId.clear();
        result.connection.relayAllowed = -1;
    }
    return result;
}

bool CoreTargetEditor::validate()
{
    const QString address = m_addressEdit->text().trimmed();
    const bool serviceOnly = address.isEmpty()
        && !m_initial.connection.identityFingerprint.isEmpty();
    RemoteStationOptions candidate = target().connection;
    if (!candidate.isValidRemoteTarget() || (!serviceOnly
        && !RemoteStationOptions::isValidStationUrl(address))) {
        // Keep this fixed: QUrl's detailed error can reflect untrusted input.
        m_errorLabel->setText(tr("Enter a valid Core address beginning with ws:// or wss://."));
        m_errorLabel->setVisible(true);
        return false;
    }
    m_errorLabel->clear();
    m_errorLabel->setVisible(false);
    return true;
}

} // namespace NereusSDR
