// =================================================================
// src/gui/setup/AudioTxInputPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → TX Input page.
// See AudioTxInputPage.h for the full header.
//
// Phase 3M-1b Task I.1 (2026-04-28): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "AudioTxInputPage.h"

#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/BoardCapabilities.h"

#include <QButtonGroup>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NereusSDR {

AudioTxInputPage::AudioTxInputPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Input"), model, parent)
{
    const bool hasMicJack = model
        ? model->boardCapabilities().hasMicJack
        : true;  // safe default: don't disable Radio Mic for null model

    buildPage(hasMicJack);

    // Wire two-way sync with TransmitModel.
    if (model) {
        TransmitModel* tx = &model->transmitModel();

        // Model → UI: reflect external setMicSource() calls into the buttons.
        connect(tx, &TransmitModel::micSourceChanged,
                this, &AudioTxInputPage::onModelMicSourceChanged);

        // Apply the current model state at construction.
        syncButtonsFromModel(tx->micSource());
    }
}

void AudioTxInputPage::buildPage(bool hasMicJack)
{
    // ── Mic Source group box ─────────────────────────────────────────────────
    auto* grp = new QGroupBox(QStringLiteral("Mic Source"), this);
    auto* grpLayout = new QVBoxLayout(grp);

    m_pcMicBtn    = new QRadioButton(QStringLiteral("PC Mic"), grp);
    m_radioMicBtn = new QRadioButton(QStringLiteral("Radio Mic"), grp);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->addButton(m_pcMicBtn,    static_cast<int>(MicSource::Pc));
    m_buttonGroup->addButton(m_radioMicBtn, static_cast<int>(MicSource::Radio));

    // PC Mic is selected by default.
    m_pcMicBtn->setChecked(true);

    // Gate Radio Mic on hasMicJack capability.
    if (!hasMicJack) {
        m_radioMicBtn->setEnabled(false);
        m_radioMicBtn->setToolTip(
            QStringLiteral("Radio mic jack not present on Hermes Lite 2"));
    }

    grpLayout->addWidget(m_pcMicBtn);
    grpLayout->addWidget(m_radioMicBtn);

    contentLayout()->insertWidget(0, grp);

    // ── Info label ───────────────────────────────────────────────────────────
    auto* infoLabel = new QLabel(
        QStringLiteral("Configure mic input. Detailed settings (PC backend, "
                       "radio mic-jack flags, mic gain) appear in I.2–I.4 "
                       "sub-tasks."),
        this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color: #7090a0; font-size: 11px;"));
    contentLayout()->addWidget(infoLabel);

    // UI → Model: user toggles a radio button.
    // QButtonGroup::idToggled fires for both the newly-checked and newly-
    // unchecked buttons. Guard with m_updatingFromModel to avoid echoing.
    connect(m_buttonGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onButtonToggled);
}

// ── Slot: UI → Model ─────────────────────────────────────────────────────────

void AudioTxInputPage::onButtonToggled(int id, bool checked)
{
    if (m_updatingFromModel) { return; }
    if (!checked) { return; }  // only act on the newly-selected button

    if (!model()) { return; }

    const MicSource source = static_cast<MicSource>(id);
    model()->transmitModel().setMicSource(source);
}

// ── Slot: Model → UI ─────────────────────────────────────────────────────────

void AudioTxInputPage::onModelMicSourceChanged(MicSource source)
{
    syncButtonsFromModel(source);
}

void AudioTxInputPage::syncButtonsFromModel(MicSource source)
{
    if (!m_buttonGroup) { return; }

    m_updatingFromModel = true;
    QAbstractButton* btn = m_buttonGroup->button(static_cast<int>(source));
    if (btn) {
        btn->setChecked(true);
    }
    m_updatingFromModel = false;
}

} // namespace NereusSDR
