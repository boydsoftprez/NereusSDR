// =================================================================
// src/gui/applets/TxCfcDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmCFCConfig.{cs,Designer.cs}
//   [v2.10.3.13] — per-band CFC editor (Continuous Frequency
//   Compressor).  Original licence reproduced below.
//
// Layout faithfully mirrors Thetis frmCFCConfig per-row field set
// (frequency / compression / post-EQ gain) with NereusSDR-spin: a 10-row
// grid replaces Thetis's selected-band + edit-pane.  Same field set,
// same value ranges, same default values.
//
// Out-of-scope (deferred to follow-up PRs):
//   - Para EQ live preview (Thetis ucCFC_eq / ucCFC_comp UC components).
//   - Q-factor toggle (chkCFC_UseQFactors) — TXA implementation pending.
//   - High/Low frequency clamps (udCFC_low / udCFC_high) — wired in TM but
//     no UI surface here yet.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task A): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
// =================================================================

//=================================================================
// frmCFCConfig.cs
//=================================================================
//  frmCFCConfig.cs
//
// This file is part of a program that implements a Software-Defined Radio.
//
// This code/file can be found on GitHub : https://github.com/ramdor/Thetis
//
// Copyright (C) 2020-2026 Richard Samphire MW0LGE
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// The author can be reached by email at
//
// mw0lge@grange-lane.co.uk
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "TxCfcDialog.h"

#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// Sentinel property holding the band index 0..9 attached to each per-band
// spinbox so the shared sender-finder slots can resolve which band fired.
constexpr const char* kBandIndexProp = "txCfcBandIndex";

} // namespace

// ─────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────

TxCfcDialog::TxCfcDialog(TransmitModel* tm,
                         MicProfileManager* mgr,
                         QWidget* parent)
    : QDialog(parent)
    , m_tm(tm)
    , m_mgr(mgr)
{
    setWindowTitle(tr("CFC — Continuous Frequency Compressor"));
    setObjectName(QStringLiteral("TxCfcDialog"));
    // Modeless: don't block other interaction.
    setModal(false);
    // Singleton-ish lifecycle — TxApplet keeps the pointer alive.  Force
    // WA_DeleteOnClose false so close-button or programmatic hide preserves
    // the instance.
    setAttribute(Qt::WA_DeleteOnClose, false);

    buildUi();
    wireSignals();
    syncFromModel();
    refreshProfileCombo();
}

TxCfcDialog::~TxCfcDialog() = default;

// ─────────────────────────────────────────────────────────────────────
// UI build-out
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // ── Profile-bank row: combo + Save / Save As / Delete ─────────
    // Mirrors TxEqDialog Batch-A.2 row exactly (same MicProfileManager
    // bank — switching a profile updates EQ AND CFC AND mic/VOX/Lev/ALC,
    // but only EQ + CFC have visible UI in this dialog).
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        auto* lbl = new QLabel(tr("Profile:"), this);
        row->addWidget(lbl);

        m_profileCombo = new QComboBox(this);
        m_profileCombo->setObjectName(QStringLiteral("TxCfcProfileCombo"));
        m_profileCombo->setEditable(false);
        m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_profileCombo->setToolTip(tr(
            "Active TX profile.  Switching loads the profile's saved CFC "
            "shape (and silently updates EQ / mic gain / VOX / Leveler / ALC "
            "— same profile bank as the TX-Profile combo on the TX applet)."));
        row->addWidget(m_profileCombo, 1);

        m_saveBtn = new QPushButton(tr("Save"), this);
        m_saveBtn->setObjectName(QStringLiteral("TxCfcProfileSaveBtn"));
        m_saveBtn->setToolTip(tr(
            "Overwrite the active profile with the current CFC + EQ + "
            "mic + VOX + Leveler + ALC state.  Confirms before overwriting."));
        row->addWidget(m_saveBtn);

        m_saveAsBtn = new QPushButton(tr("Save As..."), this);
        m_saveAsBtn->setObjectName(QStringLiteral("TxCfcProfileSaveAsBtn"));
        m_saveAsBtn->setToolTip(tr(
            "Save the current state under a new profile name."));
        row->addWidget(m_saveAsBtn);

        m_deleteBtn = new QPushButton(tr("Delete"), this);
        m_deleteBtn->setObjectName(QStringLiteral("TxCfcProfileDeleteBtn"));
        m_deleteBtn->setToolTip(tr(
            "Delete the active profile.  The last remaining profile "
            "cannot be deleted."));
        row->addWidget(m_deleteBtn);

        outer->addLayout(row);
    }

    auto* sepProfile = new QFrame(this);
    sepProfile->setFrameShape(QFrame::HLine);
    sepProfile->setFrameShadow(QFrame::Sunken);
    outer->addWidget(sepProfile);

    // ── Globals row: Pre-Comp + Post-EQ Gain ──────────────────────
    // From frmCFCConfig.Designer.cs:408-422 [v2.10.3.13] (nudCFC_precomp:
    // 0..16 dB) and :337-351 (nudCFC_posteqgain: -24..+24 dB).
    {
        auto* globalsGrp = new QGroupBox(tr("Global"), this);
        auto* row = new QHBoxLayout(globalsGrp);
        row->setContentsMargins(8, 14, 8, 8);
        row->setSpacing(8);

        {
            auto* lbl = new QLabel(tr("Pre-Comp:"), globalsGrp);
            row->addWidget(lbl);
            m_precompSpin = new QSpinBox(globalsGrp);
            m_precompSpin->setObjectName(QStringLiteral("TxCfcPrecompSpin"));
            m_precompSpin->setRange(TransmitModel::kCfcPrecompDbMin,
                                    TransmitModel::kCfcPrecompDbMax);
            m_precompSpin->setSuffix(QStringLiteral(" dB"));
            m_precompSpin->setToolTip(tr(
                "Global pre-compression gain (0–16 dB) applied before "
                "per-band compression."));
            row->addWidget(m_precompSpin);
        }

        row->addSpacing(20);

        {
            auto* lbl = new QLabel(tr("Post-EQ Gain:"), globalsGrp);
            row->addWidget(lbl);
            m_postEqGainSpin = new QSpinBox(globalsGrp);
            m_postEqGainSpin->setObjectName(QStringLiteral("TxCfcPostEqGainSpin"));
            m_postEqGainSpin->setRange(TransmitModel::kCfcPostEqGainDbMin,
                                       TransmitModel::kCfcPostEqGainDbMax);
            m_postEqGainSpin->setSuffix(QStringLiteral(" dB"));
            m_postEqGainSpin->setToolTip(tr(
                "Global Post-EQ make-up gain (-24 to +24 dB)."));
            row->addWidget(m_postEqGainSpin);
        }

        row->addStretch(1);
        outer->addWidget(globalsGrp);
    }

    // ── 10-row per-band grid: # | Freq Hz | Comp dB | Post-EQ dB ──
    // Each row is a band 1..10.  Cells:
    //   FREQ:    range 0..20000 Hz       (frmCFCConfig.Designer.cs:267-286)
    //   COMP:    range 0..16 dB          (frmCFCConfig.Designer.cs:217-236)
    //   POST-EQ: range -24..+24 dB       (frmCFCConfig.Designer.cs:564-583)
    {
        auto* bandsGrp = new QGroupBox(tr("Per-band CFC"), this);
        auto* grid = new QGridLayout(bandsGrp);
        grid->setContentsMargins(8, 14, 8, 8);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);

        // Header row
        auto* h0 = new QLabel(tr("#"), bandsGrp);
        auto* h1 = new QLabel(tr("Freq (Hz)"), bandsGrp);
        auto* h2 = new QLabel(tr("Comp (dB)"), bandsGrp);
        auto* h3 = new QLabel(tr("Post-EQ (dB)"), bandsGrp);
        QFont hf = h0->font();
        hf.setBold(true);
        h0->setFont(hf); h1->setFont(hf); h2->setFont(hf); h3->setFont(hf);
        grid->addWidget(h0, 0, 0, Qt::AlignHCenter);
        grid->addWidget(h1, 0, 1, Qt::AlignHCenter);
        grid->addWidget(h2, 0, 2, Qt::AlignHCenter);
        grid->addWidget(h3, 0, 3, Qt::AlignHCenter);

        for (int i = 0; i < 10; ++i) {
            const int row = i + 1;
            auto* num = new QLabel(QString::number(i + 1), bandsGrp);
            num->setAlignment(Qt::AlignCenter);
            grid->addWidget(num, row, 0);

            // FREQ
            auto* fSpin = new QSpinBox(bandsGrp);
            fSpin->setObjectName(QStringLiteral("TxCfcFreqSpin%1").arg(i));
            fSpin->setRange(TransmitModel::kCfcEqFreqHzMin,
                            TransmitModel::kCfcEqFreqHzMax);
            fSpin->setSuffix(QStringLiteral(" Hz"));
            fSpin->setProperty(kBandIndexProp, i);
            fSpin->setMinimumWidth(96);
            fSpin->setToolTip(tr("Band %1 center frequency (0–20000 Hz).").arg(i + 1));
            grid->addWidget(fSpin, row, 1);
            m_freqSpins[i] = fSpin;

            // COMP (compression)
            auto* cSpin = new QSpinBox(bandsGrp);
            cSpin->setObjectName(QStringLiteral("TxCfcCompSpin%1").arg(i));
            cSpin->setRange(TransmitModel::kCfcCompressionDbMin,
                            TransmitModel::kCfcCompressionDbMax);
            cSpin->setSuffix(QStringLiteral(" dB"));
            cSpin->setProperty(kBandIndexProp, i);
            cSpin->setToolTip(tr("Band %1 compression amount (0–16 dB).").arg(i + 1));
            grid->addWidget(cSpin, row, 2);
            m_compSpins[i] = cSpin;

            // POST-EQ band gain
            auto* gSpin = new QSpinBox(bandsGrp);
            gSpin->setObjectName(QStringLiteral("TxCfcPostEqBandSpin%1").arg(i));
            gSpin->setRange(TransmitModel::kCfcPostEqBandGainDbMin,
                            TransmitModel::kCfcPostEqBandGainDbMax);
            gSpin->setSuffix(QStringLiteral(" dB"));
            gSpin->setProperty(kBandIndexProp, i);
            gSpin->setToolTip(tr("Band %1 post-EQ gain (-24 to +24 dB).").arg(i + 1));
            grid->addWidget(gSpin, row, 3);
            m_postEqBandSpins[i] = gSpin;
        }

        outer->addWidget(bandsGrp, 1);
    }

    // ── Bottom strip: Reset to defaults + Close ───────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        m_resetBtn = new QPushButton(tr("Reset to factory defaults"), this);
        m_resetBtn->setObjectName(QStringLiteral("TxCfcResetBtn"));
        m_resetBtn->setToolTip(tr(
            "Restore CFC values from the bundled \"Default\" factory profile.  "
            "Confirms before overwriting current values."));
        row->addWidget(m_resetBtn);

        row->addStretch(1);

        m_closeBtn = new QPushButton(tr("Close"), this);
        m_closeBtn->setObjectName(QStringLiteral("TxCfcCloseBtn"));
        m_closeBtn->setAutoDefault(false);
        m_closeBtn->setDefault(false);
        row->addWidget(m_closeBtn);

        outer->addLayout(row);
    }

    setMinimumWidth(560);
}

// ─────────────────────────────────────────────────────────────────────
// Signal wiring
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::wireSignals()
{
    // ── UI → model ────────────────────────────────────────────────

    // Globals
    connect(m_precompSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int dB) {
        if (m_updatingFromModel || !m_tm) { return; }
        m_tm->setCfcPrecompDb(dB);
    });

    connect(m_postEqGainSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int dB) {
        if (m_updatingFromModel || !m_tm) { return; }
        m_tm->setCfcPostEqGainDb(dB);
    });

    // Per-band spinboxes — use shared sender-finder slots per the
    // kBandIndexProp sentinel.
    for (int i = 0; i < 10; ++i) {
        connect(m_freqSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int hz) {
            if (m_updatingFromModel || !m_tm) { return; }
            QObject* s = sender();
            if (!s) { return; }
            bool ok = false;
            const int idx = s->property(kBandIndexProp).toInt(&ok);
            if (!ok || idx < 0 || idx >= 10) { return; }
            m_tm->setCfcEqFreq(idx, hz);
        });

        connect(m_compSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int dB) {
            if (m_updatingFromModel || !m_tm) { return; }
            QObject* s = sender();
            if (!s) { return; }
            bool ok = false;
            const int idx = s->property(kBandIndexProp).toInt(&ok);
            if (!ok || idx < 0 || idx >= 10) { return; }
            m_tm->setCfcCompression(idx, dB);
        });

        connect(m_postEqBandSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int dB) {
            if (m_updatingFromModel || !m_tm) { return; }
            QObject* s = sender();
            if (!s) { return; }
            bool ok = false;
            const int idx = s->property(kBandIndexProp).toInt(&ok);
            if (!ok || idx < 0 || idx >= 10) { return; }
            m_tm->setCfcPostEqBandGain(idx, dB);
        });
    }

    // Profile-bank buttons
    connect(m_profileCombo, &QComboBox::currentTextChanged,
            this, &TxCfcDialog::onProfileComboChanged);
    connect(m_saveBtn,   &QPushButton::clicked, this, &TxCfcDialog::onSaveClicked);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &TxCfcDialog::onSaveAsClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &TxCfcDialog::onDeleteClicked);
    connect(m_resetBtn,  &QPushButton::clicked, this, &TxCfcDialog::onResetDefaultsClicked);
    connect(m_closeBtn,  &QPushButton::clicked, this, &QDialog::hide);

    // ── Model → UI ────────────────────────────────────────────────
    if (m_tm) {
        connect(m_tm.data(), &TransmitModel::cfcPrecompDbChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcPostEqGainDbChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcEqFreqChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcCompressionChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcPostEqBandGainChanged,
                this, &TxCfcDialog::syncFromModel);
    }

    // ── Profile manager ↔ combo ──────────────────────────────────
    if (m_mgr) {
        connect(m_mgr.data(), &MicProfileManager::profileListChanged,
                this, &TxCfcDialog::refreshProfileCombo);
        connect(m_mgr.data(), &MicProfileManager::activeProfileChanged,
                this, &TxCfcDialog::onActiveProfileChanged);
    }
}

// ─────────────────────────────────────────────────────────────────────
// Model → UI sync (echo-guarded)
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::syncFromModel()
{
    if (!m_tm) { return; }

    m_updatingFromModel = true;

    {
        QSignalBlocker b(m_precompSpin);
        m_precompSpin->setValue(m_tm->cfcPrecompDb());
    }
    {
        QSignalBlocker b(m_postEqGainSpin);
        m_postEqGainSpin->setValue(m_tm->cfcPostEqGainDb());
    }
    for (int i = 0; i < 10; ++i) {
        {
            QSignalBlocker bf(m_freqSpins[i]);
            m_freqSpins[i]->setValue(m_tm->cfcEqFreq(i));
        }
        {
            QSignalBlocker bc(m_compSpins[i]);
            m_compSpins[i]->setValue(m_tm->cfcCompression(i));
        }
        {
            QSignalBlocker bg(m_postEqBandSpins[i]);
            m_postEqBandSpins[i]->setValue(m_tm->cfcPostEqBandGain(i));
        }
    }

    m_updatingFromModel = false;
}

// ─────────────────────────────────────────────────────────────────────
// Profile-bank handlers (mirror TxEqDialog A.2)
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::refreshProfileCombo()
{
    if (!m_profileCombo || !m_mgr) { return; }

    QSignalBlocker blk(m_profileCombo);
    const QString prevSelection = m_profileCombo->currentText();
    m_profileCombo->clear();
    const QStringList names = m_mgr->profileNames();
    m_profileCombo->addItems(names);

    const QString active = m_mgr->activeProfileName();
    int idx = m_profileCombo->findText(active);
    if (idx < 0) {
        idx = m_profileCombo->findText(prevSelection);
    }
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    } else if (!names.isEmpty()) {
        m_profileCombo->setCurrentIndex(0);
    }
}

void TxCfcDialog::onActiveProfileChanged(const QString& name)
{
    if (!m_profileCombo) { return; }
    QSignalBlocker blk(m_profileCombo);
    const int idx = m_profileCombo->findText(name);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }
}

void TxCfcDialog::onProfileComboChanged(const QString& name)
{
    if (!m_tm || !m_mgr || name.isEmpty()) { return; }
    // setActiveProfile pushes the saved values into TransmitModel; the
    // model's *Changed signals fan out to syncFromModel which redraws
    // the visible CFC controls.
    m_mgr->setActiveProfile(name, m_tm.data());
}

bool TxCfcDialog::persistProfile(const QString& name, bool setActiveAfter)
{
    if (!m_tm || !m_mgr || name.isEmpty()) { return false; }
    const bool ok = m_mgr->saveProfile(name, m_tm.data());
    if (ok && setActiveAfter) {
        m_mgr->setActiveProfile(name, m_tm.data());
    }
    return ok;
}

void TxCfcDialog::onSaveClicked()
{
    if (!m_tm || !m_mgr) { return; }

    const QString active = m_mgr->activeProfileName();
    if (active.isEmpty()) {
        onSaveAsClicked();
        return;
    }

    bool overwrite = false;
    if (m_overwriteHook) {
        overwrite = m_overwriteHook(active);
    } else {
        const auto answer = QMessageBox::question(
            this, tr("Overwrite TX Profile"),
            tr("Overwrite profile \"%1\" with the current settings?")
                .arg(active),
            QMessageBox::Yes | QMessageBox::No);
        overwrite = (answer == QMessageBox::Yes);
    }
    if (!overwrite) { return; }

    persistProfile(active, /*setActiveAfter=*/false);
}

void TxCfcDialog::onSaveAsClicked()
{
    if (!m_tm || !m_mgr) { return; }

    const QString seed = m_mgr->activeProfileName();

    QString rawName;
    bool accepted = false;
    if (m_saveAsHook) {
        const auto result = m_saveAsHook(seed);
        accepted = result.first;
        rawName = result.second;
    } else {
        bool ok = false;
        rawName = QInputDialog::getText(
            this, tr("Save TX Profile As"),
            tr("Enter new TX profile name:"),
            QLineEdit::Normal, seed, &ok);
        accepted = ok;
    }
    if (!accepted) { return; }

    // Strip commas (Thetis precedent: setup.cs:9550-9552 [v2.10.3.13]
    // — TCI safety; mirrors TxEqDialog handling).
    QString name = rawName;
    name.replace(QLatin1Char(','), QLatin1Char('_'));
    name = name.trimmed();
    if (name.isEmpty()) { return; }

    const QStringList existing = m_mgr->profileNames();
    if (existing.contains(name)) {
        bool overwrite = false;
        if (m_overwriteHook) {
            overwrite = m_overwriteHook(name);
        } else {
            const auto answer = QMessageBox::question(
                this, tr("Overwrite TX Profile"),
                tr("A profile named \"%1\" already exists.  Overwrite?")
                    .arg(name),
                QMessageBox::Yes | QMessageBox::No);
            overwrite = (answer == QMessageBox::Yes);
        }
        if (!overwrite) { return; }
    }

    persistProfile(name, /*setActiveAfter=*/true);
}

void TxCfcDialog::onDeleteClicked()
{
    if (!m_mgr) { return; }

    const QString target = m_mgr->activeProfileName();
    if (target.isEmpty()) { return; }

    bool confirmed = false;
    if (m_deleteHook) {
        confirmed = m_deleteHook(target);
    } else {
        const auto answer = QMessageBox::question(
            this, tr("Delete TX Profile"),
            tr("Delete profile \"%1\"?").arg(target),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }
    if (!confirmed) { return; }

    const bool ok = m_mgr->deleteProfile(target);
    if (!ok) {
        // Last-remaining guard fires.  Surface the verbatim Thetis
        // string per setup.cs:9617-9624 [v2.10.3.13] — same wording
        // TxEqDialog uses (the message is profile-bank-wide, not
        // EQ-specific).
        const QString msg = QStringLiteral(
            "It is not possible to delete the last remaining TX profile");
        if (m_rejectionHook) {
            m_rejectionHook(msg);
        } else {
            QMessageBox::information(this,
                                      tr("Cannot Delete Profile"), msg);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// Reset to factory defaults
//
// Strategy: re-apply the Default profile via MicProfileManager.  This is the
// same path TxEqDialog uses (setActiveProfile pushes saved values back into
// TransmitModel via applyValuesToModel; syncFromModel then redraws the UI).
// "Default" is seeded by MicProfileManager::load() on first launch from
// defaultProfileValues() (database.cs:4724-4768 [v2.10.3.13]) so the values
// are guaranteed to exist.
// ─────────────────────────────────────────────────────────────────────
void TxCfcDialog::onResetDefaultsClicked()
{
    if (!m_tm || !m_mgr) { return; }

    bool confirmed = false;
    if (m_resetHook) {
        confirmed = m_resetHook();
    } else {
        const auto answer = QMessageBox::question(
            this, tr("Reset CFC to Factory Defaults"),
            tr("Reset all CFC values from the \"Default\" factory profile?"),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }
    if (!confirmed) { return; }

    // Re-apply the Default profile.  setActiveProfile fans the saved values
    // back into TransmitModel; the model's *Changed signals pull syncFromModel
    // which redraws the visible CFC controls.
    m_mgr->setActiveProfile(QStringLiteral("Default"), m_tm.data());
}

// ─────────────────────────────────────────────────────────────────────
// Test hook setters
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::setSaveAsPromptHook(SaveAsPromptHook hook)
{
    m_saveAsHook = std::move(hook);
}

void TxCfcDialog::setOverwriteConfirmHook(OverwriteConfirmHook hook)
{
    m_overwriteHook = std::move(hook);
}

void TxCfcDialog::setDeleteConfirmHook(DeleteConfirmHook hook)
{
    m_deleteHook = std::move(hook);
}

void TxCfcDialog::setResetConfirmHook(ResetConfirmHook hook)
{
    m_resetHook = std::move(hook);
}

void TxCfcDialog::setRejectionMessageHook(RejectionMessageHook hook)
{
    m_rejectionHook = std::move(hook);
}

} // namespace NereusSDR
