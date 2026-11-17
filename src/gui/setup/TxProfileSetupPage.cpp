// =================================================================
// src/gui/setup/TxProfileSetupPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → TX Profile editor page.
// See TxProfileSetupPage.h for the full header + Thetis-semantics cites.
//
// Phase 3M-1c chunk J.3 + J.4.
//
// Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-25 - R-R3-49 (parity Task 3): the transmit settings gates for
//                 a remote window, each control on the version that brought
//                 it. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: Setup description version 15 ids on
//                the profile choice, Save and Delete; Save's tooltip says
//                "transmit settings". J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-08  J.J. Boyd (KG4VCF): Qt 6.11.0 Cocoa accessibility cache
//               repair on a live item view (QtCocoaAccessibleTableCache).
//               AI-assisted via Anthropic Claude Code.
//   2026-10-15  J.J. Boyd (KG4VCF): rows change after a real model reset
//               (resetItemsBeforeRowChange), so accessibility cannot read a
//               deleted cell. AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived semantics are
// cited inline in the header.

#include "TxProfileSetupPage.h"
#include "gui/QtCocoaAccessibleTableCache.h"

#include "core/MicProfileManager.h"
#include "core/TxProfileFile.h"
#include "core/session/IStationLink.h"
#include "core/session/StationCapabilities.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TxProfileSetupPage::TxProfileSetupPage(RadioModel* model,
                                         MicProfileManager* profileMgr,
                                         TransmitModel* tx,
                                         QWidget* parent)
    : SetupPage(QStringLiteral("TX Profile"), model, parent)
    , m_profileMgr(profileMgr)
    , m_tx(tx)
{
    buildUi();
    rebuildCombo();
    wireDirtyTracking();

    // R-R3-49 (parity Task 3): in a remote window the controls start closed
    // until the Core says it takes them (SetupDialog pushes each version).
    if (model && !model->ownsLocalDsp()) {
        setTransmitSettingsPermitted(false, QString());
        setTransmitSettingsPermittedAt(2, false, QString());
        setTransmitSettingsPermittedAt(3, false, QString());
        setTransmitSettingsPermittedAt(kTransmitSettingsTxProfileFileVersion, false, QString());
    }

    if (m_profileMgr) {
        // Refresh on external add/remove (e.g. CAT-imported profile).
        connect(m_profileMgr, &MicProfileManager::profileListChanged,
                this, &TxProfileSetupPage::rebuildCombo);
        // Refresh selection on external active-profile change.
        connect(m_profileMgr, &MicProfileManager::activeProfileChanged,
                this, [this](const QString& name) {
            if (!m_combo) { return; }
            QSignalBlocker blk(m_combo);
            m_seedingFromModel = true;
            const int idx = m_combo->findText(name);
            if (idx >= 0) { m_combo->setCurrentIndex(idx); }
            m_lastComboText = name;
            m_seedingFromModel = false;
            clearDirty();
        });
    }
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void TxProfileSetupPage::buildUi()
{
    QGroupBox* group = addSection(QStringLiteral("TX Profile"));

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);

    // Profile combo (editable so user could in principle type a new name —
    // but the canonical "Save" path is via the Save button).
    m_combo = new QComboBox(group);
    m_combo->setEditable(false);  // restrict edits to Save button path
    m_combo->setProperty("nereusSetupId", "audio.txProfile.activeProfile");
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_combo->setToolTip(QStringLiteral(
        "Active TX Profile.  Switching prompts to save unsaved changes."));
    form->addRow(QStringLiteral("Active profile:"), m_combo);

    // Save / Delete buttons row.
    auto* btnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton(QStringLiteral("Save..."), group);
    m_saveBtn->setProperty("nereusSetupId", "audio.txProfile.save");
    m_saveBtn->setToolTip(QStringLiteral(
        "Save the current transmit settings under a profile name."));
    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), group);
    m_deleteBtn->setProperty("nereusSetupId", "audio.txProfile.delete");
    m_deleteBtn->setToolTip(QStringLiteral(
        "Delete the currently-selected profile."));
    m_exportBtn = new QPushButton(QStringLiteral("Export…"), group);
    m_exportBtn->setObjectName(QStringLiteral("exportTxProfileButton"));
    m_exportBtn->setToolTip(QStringLiteral(
        "Save the selected transmit profile to a file."));
    m_importBtn = new QPushButton(QStringLiteral("Import…"), group);
    m_importBtn->setObjectName(QStringLiteral("importTxProfileButton"));
    m_importBtn->setToolTip(QStringLiteral(
        "Add a transmit profile from a file and select it."));
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_exportBtn);
    btnRow->addWidget(m_importBtn);
    btnRow->addStretch(1);
    form->addRow(QString(), btnRow);

    if (auto* vlay = qobject_cast<QVBoxLayout*>(group->layout())) {
        vlay->addLayout(form);
    }

    connect(m_combo, &QComboBox::currentTextChanged,
            this, &TxProfileSetupPage::onComboTextChanged);
    connect(m_saveBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onSaveClicked);
    connect(m_deleteBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onDeleteClicked);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onExportClicked);
    connect(m_importBtn, &QPushButton::clicked,
            this, &TxProfileSetupPage::onImportClicked);

    // ── Plan 4 Cluster C (Task 5 / D6): TX Filter group ──────────────────────
    // Low/High cutoff spinboxes for the TX bandpass filter.  Bidirectional with
    // TransmitModel::filterLow / filterHigh via filterChanged(int,int).
    // Cross-surface sync with TxApplet spinboxes happens automatically through
    // the shared TransmitModel::filterChanged signal — no extra wiring needed.
    if (m_tx) {
        QGroupBox* filterGroup = addSection(QStringLiteral("TX Filter"));
        auto* filterForm = new QFormLayout();
        filterForm->setContentsMargins(0, 0, 0, 0);

        auto* lowSpin = new QSpinBox(filterGroup);
        m_filterLowSpin = lowSpin;
        lowSpin->setProperty("nereusSetupId", QStringLiteral("audio.txProfile.filterLow"));
        lowSpin->setRange(0, 5000);
        lowSpin->setSuffix(QStringLiteral(" Hz"));
        lowSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        lowSpin->setToolTip(QStringLiteral(
            "TX bandpass filter lower cutoff (Hz).  0 Hz for voice SSB modes."));
        lowSpin->setValue(m_tx->filterLow());
        filterForm->addRow(QStringLiteral("Low cutoff:"), lowSpin);

        auto* highSpin = new QSpinBox(filterGroup);
        m_filterHighSpin = highSpin;
        highSpin->setProperty("nereusSetupId", QStringLiteral("audio.txProfile.filterHigh"));
        highSpin->setRange(200, 10000);
        highSpin->setSuffix(QStringLiteral(" Hz"));
        highSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        highSpin->setToolTip(QStringLiteral(
            "TX bandpass filter upper cutoff (Hz).  2900 Hz for typical SSB voice."));
        highSpin->setValue(m_tx->filterHigh());
        filterForm->addRow(QStringLiteral("High cutoff:"), highSpin);

        // AM / SAM / DSB carrier level (Thetis TXProfile AM_Carrier_Level).
        auto* carrierSpin = new QSpinBox(filterGroup);
        m_amCarrierSpin = carrierSpin;
        carrierSpin->setProperty("nereusSetupId", QStringLiteral("audio.txProfile.amCarrierLevel"));
        carrierSpin->setRange(TransmitModel::kAmCarrierLevelMin,
                              TransmitModel::kAmCarrierLevelMax);
        carrierSpin->setSuffix(QStringLiteral(" %"));
        carrierSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        // 100 % = full carrier, as in Thetis.
        carrierSpin->setToolTip(QStringLiteral(
            "AM / SAM / DSB carrier level (%).  100 % = full carrier.  "
            "In AM the TX filter is symmetric: +/- High cutoff around the carrier."));
        carrierSpin->setValue(m_tx->amCarrierLevel());
        filterForm->addRow(QStringLiteral("AM carrier level:"), carrierSpin);

        if (auto* vlay = qobject_cast<QVBoxLayout*>(filterGroup->layout())) {
            vlay->addLayout(filterForm);
        }

        connect(carrierSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_tx, &TransmitModel::setAmCarrierLevel);
        connect(m_tx, &TransmitModel::amCarrierLevelChanged,
                this, [carrierSpin](int pct) {
            QSignalBlocker b(carrierSpin);
            carrierSpin->setValue(pct);
        });

        // UI → Model
        connect(lowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_tx, &TransmitModel::setFilterLow);
        connect(highSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_tx, &TransmitModel::setFilterHigh);

        // Model → UI (filterChanged carries both values)
        connect(m_tx, &TransmitModel::filterChanged,
                this, [lowSpin, highSpin](int low, int high) {
            {
                QSignalBlocker bLo(lowSpin);
                lowSpin->setValue(low);
            }
            {
                QSignalBlocker bHi(highSpin);
                highSpin->setValue(high);
            }
        });
    }
}

// ---------------------------------------------------------------------------
// rebuildCombo — populate from MicProfileManager.  Programmatic update;
// no prompt firing.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::rebuildCombo()
{
    if (!m_combo) { return; }

    QSignalBlocker blk(m_combo);
    m_seedingFromModel = true;

    resetItemsBeforeRowChange(m_combo);
    m_combo->clear();
    if (m_profileMgr) {
        const QStringList names = m_profileMgr->profileNames();
        m_combo->addItems(names);
        const int idx = m_combo->findText(m_profileMgr->activeProfileName());
        if (idx >= 0) {
            m_combo->setCurrentIndex(idx);
            m_lastComboText = m_combo->currentText();
        } else if (!names.isEmpty()) {
            m_combo->setCurrentIndex(0);
            m_lastComboText = names.first();
        }
    }

    m_seedingFromModel = false;
    clearDirty();
}

// ---------------------------------------------------------------------------
// J.4 dirty tracking
// ---------------------------------------------------------------------------
//
// We connect to TransmitModel's *Changed signals.  Any emission while
// m_seedingFromModel == false marks the page dirty.  The dirty state is
// cleared on save / load / activeProfileChanged.
//
// The signal set covers the 23 live keys MicProfileManager captures
// (mic / VOX / MON / two-tone), plus a few non-captured but UI-relevant
// signals.  False positives (e.g. tunePowerByBandChanged) are acceptable
// in a v1 editor — the only consequence is an extra prompt.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::wireDirtyTracking()
{
    if (!m_tx) { return; }

    auto markDirty = [this]() {
        if (!m_seedingFromModel) {
            m_dirty = true;
        }
    };

    // Mic / VOX / MON.
    connect(m_tx, &TransmitModel::micGainDbChanged,         this, markDirty);
    connect(m_tx, &TransmitModel::micBoostChanged,          this, markDirty);
    connect(m_tx, &TransmitModel::micXlrChanged,            this, markDirty);
    connect(m_tx, &TransmitModel::lineInChanged,            this, markDirty);
    connect(m_tx, &TransmitModel::lineInBoostChanged,       this, markDirty);
    connect(m_tx, &TransmitModel::micTipRingChanged,        this, markDirty);
    connect(m_tx, &TransmitModel::micBiasChanged,           this, markDirty);
    connect(m_tx, &TransmitModel::micPttDisabledChanged,    this, markDirty);
    connect(m_tx, &TransmitModel::voxThresholdDbChanged,    this, markDirty);
    connect(m_tx, &TransmitModel::voxGainScalarChanged,     this, markDirty);
    connect(m_tx, &TransmitModel::voxHangTimeMsChanged,     this, markDirty);
    connect(m_tx, &TransmitModel::antiVoxGainDbChanged,     this, markDirty);
    // 3M-3a-iv post-bench refactor (Option A): antiVoxSourceVaxChanged dirty
    // wire dropped alongside the antiVoxSourceVax property.
    connect(m_tx, &TransmitModel::monitorVolumeChanged,     this, markDirty);
    connect(m_tx, &TransmitModel::micSourceChanged,         this, markDirty);
    connect(m_tx, &TransmitModel::amCarrierLevelChanged,    this, markDirty);  // AM/SAM/DSB carrier

    // Two-tone (7 properties + 1 enum).
    connect(m_tx, &TransmitModel::twoToneFreq1Changed,           this, markDirty);
    connect(m_tx, &TransmitModel::twoToneFreq2Changed,           this, markDirty);
    connect(m_tx, &TransmitModel::twoToneLevelChanged,           this, markDirty);
    connect(m_tx, &TransmitModel::twoTonePowerChanged,           this, markDirty);
    connect(m_tx, &TransmitModel::twoToneFreq2DelayChanged,      this, markDirty);
    connect(m_tx, &TransmitModel::twoToneInvertChanged,          this, markDirty);
    connect(m_tx, &TransmitModel::twoTonePulsedChanged,          this, markDirty);
    connect(m_tx, &TransmitModel::twoToneDrivePowerSourceChanged, this, markDirty);
}

void TxProfileSetupPage::clearDirty()
{
    m_dirty = false;
}

// ---------------------------------------------------------------------------
// applyProfileByName — load profile values into TransmitModel.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::applyProfileByName(const QString& name)
{
    if (!m_profileMgr || !m_tx) { return; }
    m_seedingFromModel = true;
    m_profileMgr->setActiveProfile(name, m_tx);
    m_seedingFromModel = false;
    clearDirty();
}

// ---------------------------------------------------------------------------
// J.4 — handleUnsavedPrompt: returns true if the switch should proceed.
// Yes → save, No → discard, Cancel → revert.
// ---------------------------------------------------------------------------
TxProfileSetupPage::UnsavedPromptResult TxProfileSetupPage::promptUnsaved()
{
    const QString currentName = m_profileMgr ? m_profileMgr->activeProfileName() : QString();
    if (m_unsavedPromptHook) {
        return m_unsavedPromptHook(currentName);
    }
    const auto answer = QMessageBox::question(
        this,
        tr("Unsaved Profile Changes"),
        tr("The current profile (\"%1\") has been modified.\n"
           "Save before switching?").arg(currentName),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    switch (answer) {
        case QMessageBox::Yes: return UnsavedPromptResult::Yes;
        case QMessageBox::No:  return UnsavedPromptResult::No;
        default:               return UnsavedPromptResult::Cancel;
    }
}

bool TxProfileSetupPage::handleUnsavedPrompt(const QString& /*newName*/)
{
    if (!m_dirty) { return true; }

    switch (promptUnsaved()) {
        case UnsavedPromptResult::Yes:
            if (m_profileMgr && m_tx && !m_profileMgr->activeProfileName().isEmpty()) {
                m_profileMgr->saveProfile(m_profileMgr->activeProfileName(), m_tx);
            }
            return true;
        case UnsavedPromptResult::No:
            return true;
        case UnsavedPromptResult::Cancel:
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Combo selection change handler.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::onComboTextChanged(const QString& newName)
{
    if (m_seedingFromModel) { return; }
    if (newName.isEmpty()) { return; }

    // Focus-gated: only run the unsaved-prompt path on user-driven changes.
    const bool userDriven = m_simulateUserChangeForTest
        || (m_combo && m_combo->hasFocus());

    if (!userDriven) {
        // Programmatic change: load unconditionally, no prompt.
        applyProfileByName(newName);
        m_lastComboText = newName;
        return;
    }

    // User-driven change.  Run the J.4 unsaved-changes prompt flow.
    if (!handleUnsavedPrompt(newName)) {
        // Cancel: revert combo to the previous selection.
        QSignalBlocker blk(m_combo);
        m_seedingFromModel = true;
        const int idx = m_combo->findText(m_lastComboText);
        if (idx >= 0) {
            m_combo->setCurrentIndex(idx);
        }
        m_seedingFromModel = false;
        return;
    }

    // Yes / No path: load the new profile.
    applyProfileByName(newName);
    m_lastComboText = newName;
}

// ---------------------------------------------------------------------------
// Save button handler.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::onSaveClicked()
{
    if (!m_profileMgr || !m_tx) { return; }

    const QString currentName = m_profileMgr->activeProfileName();

    SavePromptResult prompt;
    if (m_savePromptHook) {
        prompt = m_savePromptHook(currentName);
    } else {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this,
            tr("Save TX Profile"),
            tr("New profile name:"),
            QLineEdit::Normal,
            currentName,
            &ok);
        prompt = SavePromptResult{ok, name};
    }

    if (!prompt.accepted) { return; }
    const QString trimmed = prompt.name.trimmed();
    if (trimmed.isEmpty()) {
        // Empty / whitespace-only name → no-op (validation).
        return;
    }

    // If the name already exists, ask for overwrite confirmation.
    const QStringList existing = m_profileMgr->profileNames();
    if (existing.contains(trimmed)) {
        bool overwrite = false;
        if (m_overwriteConfirmHook) {
            overwrite = m_overwriteConfirmHook();
        } else {
            const auto answer = QMessageBox::question(
                this,
                tr("Overwrite TX Profile"),
                tr("A profile named \"%1\" already exists.  Overwrite?").arg(trimmed),
                QMessageBox::Yes | QMessageBox::No);
            overwrite = (answer == QMessageBox::Yes);
        }
        if (!overwrite) { return; }
    }

    m_profileMgr->saveProfile(trimmed, m_tx);
    clearDirty();
}

// ---------------------------------------------------------------------------
// Delete button handler.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::onDeleteClicked()
{
    if (!m_profileMgr || !m_combo) { return; }

    const QString target = m_combo->currentText();
    if (target.isEmpty()) { return; }

    // Confirm.
    bool confirmed = false;
    if (m_deleteConfirmHook) {
        confirmed = m_deleteConfirmHook();
    } else {
        const auto answer = QMessageBox::question(
            this,
            tr("Delete TX Profile"),
            tr("Delete profile \"%1\"?").arg(target),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }

    if (!confirmed) { return; }

    const bool ok = m_profileMgr->deleteProfile(target);
    if (!ok) {
        // MicProfileManager refused (last-remaining-profile guard from F.3).
        // Surface the verbatim Thetis string per chunk-J spec.
        const QString msg = QStringLiteral(
            "It is not possible to delete the last remaining TX profile");
        if (m_rejectionMessageHook) {
            m_rejectionMessageHook(msg);
        } else {
            QMessageBox::information(this, tr("Cannot Delete Profile"), msg);
        }
    }
}

// ---------------------------------------------------------------------------
// R-R3-49 (parity Task 3): the transmit settings gates in a remote window.
// SetupDialog pushes them; a local window never calls these.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::setTransmitSettingsPermitted(bool permitted, const QString& reason)
{
    // The TX filter (filterLow / filterHigh), transmitSettingsVersion 1.
    gateTransmitControls({m_filterLowSpin, m_filterHighSpin}, permitted,
                         reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason()
                                          : reason);
}

void TxProfileSetupPage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                        const QString& reason)
{
    const QString shown = reason.isEmpty()
        ? IStationLink::transmitSettingsUnavailableReason() : reason;
    if (version == 2) {
        gateTransmitControls({m_amCarrierSpin}, permitted, shown);
    } else if (version == 3) {
        gateTransmitControls({m_combo, m_saveBtn, m_deleteBtn}, permitted, shown);
    } else if (version == kTransmitSettingsTxProfileFileVersion) {
        gateTransmitControls({m_importBtn}, permitted, shown);
    }
}

// ---------------------------------------------------------------------------
// Test seam: drive a "user-driven" combo change without focus events.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::simulateUserComboChangeForTest(const QString& newName)
{
    if (!m_combo) { return; }
    m_simulateUserChangeForTest = true;
    m_combo->setCurrentText(newName);
    m_simulateUserChangeForTest = false;
}

// ---------------------------------------------------------------------------
// Test hook setters.
// ---------------------------------------------------------------------------
void TxProfileSetupPage::setSavePromptHook(SavePromptHook hook)
{
    m_savePromptHook = std::move(hook);
}

void TxProfileSetupPage::setOverwriteConfirmHook(OverwriteConfirmHook hook)
{
    m_overwriteConfirmHook = std::move(hook);
}

void TxProfileSetupPage::setDeleteConfirmHook(DeleteConfirmHook hook)
{
    m_deleteConfirmHook = std::move(hook);
}

void TxProfileSetupPage::setRejectionMessageHook(RejectionMessageHook hook)
{
    m_rejectionMessageHook = std::move(hook);
}

void TxProfileSetupPage::setUnsavedPromptHook(UnsavedPromptHook hook)
{
    m_unsavedPromptHook = std::move(hook);
}

void TxProfileSetupPage::setExportPathHook(FilePathHook hook)
{
    m_exportPathHook = std::move(hook);
}

void TxProfileSetupPage::setImportPathHook(FilePathHook hook)
{
    m_importPathHook = std::move(hook);
}

void TxProfileSetupPage::setFileResultHook(FileResultHook hook)
{
    m_fileResultHook = std::move(hook);
}

void TxProfileSetupPage::showFileResult(bool success, const QString& text)
{
    if (m_fileResultHook) {
        m_fileResultHook(success, text);
        return;
    }
    if (success) {
        QMessageBox::information(this, tr("TX Profile"), text);
    } else {
        QMessageBox::warning(this, tr("TX Profile"), text);
    }
}

QString TxProfileSetupPage::chooseExportPath(const QString& profileName)
{
    QString title = profileName;
    for (const QChar mark : {QLatin1Char('/'), QLatin1Char('\\'), QLatin1Char(':')}) {
        title.replace(mark, QLatin1Char('_'));
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export TX Profile"),
        QStringLiteral("%1.nereus-tx-profile").arg(title),
        tr("Nereus TX profile (*.nereus-tx-profile)"));
    if (!path.isEmpty() && !path.endsWith(QStringLiteral(".nereus-tx-profile"))) {
        path += QStringLiteral(".nereus-tx-profile");
    }
    return path;
}

QString TxProfileSetupPage::chooseImportPath()
{
    return QFileDialog::getOpenFileName(
        this, tr("Import TX Profile"), {},
        tr("Nereus TX profile (*.nereus-tx-profile);;All files (*)"));
}

namespace {

QHash<QString, QVariant> variantKeys(const QHash<QString, QString>& keys)
{
    QHash<QString, QVariant> values;
    for (auto it = keys.constBegin(); it != keys.constEnd(); ++it) {
        values.insert(it.key(), it.value());
    }
    return values;
}

} // namespace

void TxProfileSetupPage::onExportClicked()
{
    if (!m_profileMgr || !m_tx || !m_combo) { return; }
    const QString name = m_combo->currentText();
    if (name.isEmpty()) {
        showFileResult(false, tr("Select a transmit profile to export."));
        return;
    }
    bool useLive = false;
    if (m_dirty) {
        switch (promptUnsaved()) {
            case UnsavedPromptResult::Cancel:
                return;
            case UnsavedPromptResult::Yes:
                if (!m_profileMgr->activeProfileName().isEmpty()) {
                    m_profileMgr->saveProfile(m_profileMgr->activeProfileName(), m_tx);
                }
                clearDirty();
                useLive = true;
                break;
            case UnsavedPromptResult::No:
                break;
        }
    }
    const QString mac = model() ? model()->currentRadioMac() : QString();
    const QHash<QString, QVariant> stored = useLive
        ? MicProfileManager::liveProfileValues(m_tx)
        : MicProfileManager::readStoredProfile(mac, name);
    if (stored.isEmpty()) {
        showFileResult(false, mac.isEmpty()
            ? tr("Connect a radio to export its transmit profile.")
            : tr("This profile has no saved settings."));
        return;
    }
    TxProfileFile::Document document;
    document.name = name;
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        document.keys.insert(it.key(), it.value().toString());
    }
    const QString path = m_exportPathHook ? m_exportPathHook() : chooseExportPath(name);
    if (path.isEmpty()) { return; }
    QByteArray bytes;
    QString error;
    if (!TxProfileFile::encode(document, &bytes, &error)) {
        showFileResult(false, error.isEmpty()
            ? tr("The transmit profile was not understood.") : error);
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        showFileResult(false, tr("Could not save the transmit profile to %1.").arg(path));
        return;
    }
    showFileResult(true, tr("The transmit profile \"%1\" was exported to:\n%2").arg(name, path));
}

void TxProfileSetupPage::onImportClicked()
{
    if (!m_profileMgr || !m_tx) { return; }
    const QString path = m_importPathHook ? m_importPathHook() : chooseImportPath();
    if (path.isEmpty()) { return; }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showFileResult(false, tr("Could not read %1.").arg(path));
        return;
    }
    const QByteArray bytes = file.read(TxProfileFile::kMaxBytes + 1);
    if (bytes.size() > TxProfileFile::kMaxBytes) {
        showFileResult(false, tr("The transmit profile is too large."));
        return;
    }
    TxProfileFile::Document document;
    QString error;
    if (!TxProfileFile::decode(bytes, &document, &error)) {
        showFileResult(false, error.isEmpty()
            ? tr("This file is not a transmit profile.") : error);
        return;
    }
    const QString name = MicProfileManager::normalizedProfileName(document.name);
    if (name.isEmpty()) {
        showFileResult(false, tr("Give the transmit profile a name."));
        return;
    }
    if (m_dirty && !handleUnsavedPrompt(name)) {
        return;
    }
    if (m_profileMgr->profileNames().contains(name)) {
        bool overwrite = false;
        if (m_overwriteConfirmHook) {
            overwrite = m_overwriteConfirmHook();
        } else {
            overwrite = QMessageBox::question(
                this, tr("Overwrite TX Profile"),
                tr("A profile named \"%1\" already exists.  Overwrite?").arg(name),
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
        }
        if (!overwrite) { return; }
    }
    if (!m_profileMgr->importProfile(name, variantKeys(document.keys), m_tx)) {
        if (!m_profileMgr->isStationMirror()) {
            showFileResult(false, tr("Could not import this transmit profile. "
                                     "Connect a radio and try again."));
        }
        return;
    }
    if (!m_profileMgr->isStationMirror()) {
        showFileResult(true, tr("Imported \"%1\" and selected it.").arg(name));
    }
}

} // namespace NereusSDR
