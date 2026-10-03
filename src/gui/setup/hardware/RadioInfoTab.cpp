// =================================================================
// src/gui/setup/hardware/RadioInfoTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-46: a remote window's RX1 sample rate sets the
//                 Core's first receiver. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the disabled RX2 sample rate combo is removed.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49, R-R3-21: the one remaining rate is labelled
//                 "Sample rate (Hz):" (no RX1/RX2 words). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-39: the live rate change is asked for without
//                 waiting (setSampleRateLiveAsync). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 5: the support info's
//                 top sample rate is the protocol's, not the board row's.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Parity ruling C4: a remote window's rate change goes to
//                 every receiver and the radio's own rate, as a local
//                 window's live change does (RadioModel::
//                 requestRadioSampleRate); on an older Core, each of its
//                 receivers, with the reason on the rate box. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-IOS-18: the identity text and the support
//                 info come from radioInfoFacts, which the Core's Setup
//                 description publishes too; each readout and the copy
//                 button carry their description ids. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: the sample rate box carries its
//                 Setup description id (version 13). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Max RX (label and support info) shows the radio's
//                 reported receiver count where it gave one
//                 (BoardCapsTable::effectiveReceiverCount, read in
//                 radioInfoFacts), the count the stream pool uses. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "RadioInfoTab.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/RadioInfoFacts.h"
#include "core/SampleRateCatalog.h"
#include "core/session/IStationLink.h"
#include "gui/ComboStyle.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

RadioInfoTab::RadioInfoTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    // ── Identity group ────────────────────────────────────────────────────────
    auto* identGroup = new QGroupBox(tr("Board Identity"), this);
    auto* form = new QFormLayout(identGroup);
    form->setLabelAlignment(Qt::AlignRight);

    m_boardLabel    = new QLabel(QStringLiteral("—"), identGroup);
    m_protocolLabel = new QLabel(QStringLiteral("—"), identGroup);
    m_adcCountLabel = new QLabel(QStringLiteral("—"), identGroup);
    m_maxRxLabel    = new QLabel(QStringLiteral("—"), identGroup);
    m_firmwareLabel = new QLabel(QStringLiteral("—"), identGroup);
    m_macLabel      = new QLabel(QStringLiteral("—"), identGroup);
    m_ipLabel       = new QLabel(QStringLiteral("—"), identGroup);

    for (QLabel* lbl : {m_boardLabel, m_protocolLabel, m_adcCountLabel,
                        m_maxRxLabel, m_firmwareLabel, m_macLabel, m_ipLabel}) {
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    // R-IOS-18: the Setup description's ids for these readouts.
    m_boardLabel->setProperty("nereusSetupId", "hardware.radioInfo.board");
    m_protocolLabel->setProperty("nereusSetupId", "hardware.radioInfo.protocol");
    m_adcCountLabel->setProperty("nereusSetupId", "hardware.radioInfo.adcCount");
    m_maxRxLabel->setProperty("nereusSetupId", "hardware.radioInfo.maxRx");
    m_firmwareLabel->setProperty("nereusSetupId", "hardware.radioInfo.firmware");
    m_macLabel->setProperty("nereusSetupId", "hardware.radioInfo.mac");
    m_ipLabel->setProperty("nereusSetupId", "hardware.radioInfo.ip");

    form->addRow(tr("Board:"),      m_boardLabel);
    form->addRow(tr("Protocol:"),   m_protocolLabel);
    form->addRow(tr("ADC count:"),  m_adcCountLabel);
    form->addRow(tr("Max RX:"),     m_maxRxLabel);
    form->addRow(tr("Firmware:"),   m_firmwareLabel);
    form->addRow(tr("MAC:"),        m_macLabel);
    form->addRow(tr("IP address:"), m_ipLabel);

    outerLayout->addWidget(identGroup);

    // ── Operating parameters group ────────────────────────────────────────────
    auto* paramGroup = new QGroupBox(tr("Operating Parameters"), this);
    auto* paramForm  = new QFormLayout(paramGroup);
    paramForm->setLabelAlignment(Qt::AlignRight);

    // RX1 sample rate combo — entries populated in populate() from
    // allowedSampleRates(proto, caps, model). Matches Thetis setup.cs:847-852.
    m_sampleRateRx1Combo = new QComboBox(paramGroup);
    applyComboStyle(m_sampleRateRx1Combo);
    m_sampleRateRx1Combo->setMinimumWidth(120);
    m_sampleRateRx1Combo->setMaximumWidth(160);
    m_sampleRateRx1Combo->setProperty("nereusSetupId", "hardware.radioInfo.sampleRate");

    // Active RX count widget removed from UI 2026-05-08 — non-functional in
    // single-RX builds (capped at 1, disabled until a radio is connected).
    // The underlying RadioModel::setActiveRxCountLive coordinator stays
    // wired and re-exposes when Phase 3F multi-panadapter lands.

    paramForm->addRow(tr("Sample rate (Hz):"), m_sampleRateRx1Combo);

    outerLayout->addWidget(paramGroup);

    // Pending-reconnect banner — hidden by default. Shown when the combo
    // value differs from the active wire rate AND a radio is connected.
    // Removed when Phase C live-apply lands (PR #36); until then, users
    // must reconnect for rate changes to take effect.
    m_reconnectBanner = new QFrame(this);
    m_reconnectBanner->setFrameShape(QFrame::StyledPanel);
    m_reconnectBanner->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #3a2a10; border: 1px solid #a07020; "
        "border-radius: 3px; padding: 4px; }"));
    auto* bannerLayout = new QHBoxLayout(m_reconnectBanner);
    bannerLayout->setContentsMargins(6, 4, 6, 4);
    m_reconnectBannerLabel = new QLabel(m_reconnectBanner);
    m_reconnectBannerLabel->setStyleSheet(QStringLiteral("color: #ffcc66;"));
    m_reconnectBannerLabel->setWordWrap(true);
    bannerLayout->addWidget(m_reconnectBannerLabel);
    m_reconnectBanner->setVisible(false);
    outerLayout->addWidget(m_reconnectBanner);

    // ── ANAN-8000DLE volts/amps toggle ────────────────────────────────────────
    // Capability-gated checkbox: visible only when the connected radio is an
    // ANAN-8000D.  Controls whether the chrome title bar shows the PA voltage
    // / current readout (which Thetis gates behind HardwareSpecific.HasVolts).
    // The PA voltage widget in the chrome bar is already auto-shown/hidden by
    // the userAdc0Changed signal (MKII-class gate in P2RadioConnection); this
    // checkbox provides an additional user override for ANAN-8000D specifically.
    // Persists: HardwareAnan8000DleShowVoltsAmps ("True"/"False", default "True").
    {
        auto& s = AppSettings::instance();
        m_anan8000DleVoltsAmpsToggle = new QCheckBox(
            tr("Show volts/amps in title bar"), this);
        m_anan8000DleVoltsAmpsToggle->setToolTip(
            tr("ANAN-8000DLE only — display power supply voltage and current "
               "draw in the chrome title bar."));
        m_anan8000DleVoltsAmpsToggle->setChecked(
            s.value(QStringLiteral("HardwareAnan8000DleShowVoltsAmps"),
                    QStringLiteral("True")).toString() == QStringLiteral("True"));
        // Hidden by default; populate() shows it when the radio is an ANAN-8000D.
        m_anan8000DleVoltsAmpsToggle->setVisible(false);

        connect(m_anan8000DleVoltsAmpsToggle, &QCheckBox::toggled, this, [this](bool v) {
            AppSettings::instance().setValue(
                QStringLiteral("HardwareAnan8000DleShowVoltsAmps"),
                v ? QStringLiteral("True") : QStringLiteral("False"));
            emit anan8000DleVoltsAmpsChanged(v);
        });
        outerLayout->addWidget(m_anan8000DleVoltsAmpsToggle);
    }

    // ── Support info button ───────────────────────────────────────────────────
    // Left-aligned page-level action.  setSizePolicy(Maximum, Fixed) prevents
    // QVBoxLayout from stretching the button to the dialog's full width.
    m_copySupportInfoButton = new QPushButton(tr("Copy Support Info to Clipboard"), this);
    m_copySupportInfoButton->setProperty("nereusSetupId", "hardware.radioInfo.copySupportInfo");
    m_copySupportInfoButton->setToolTip(
        tr("Copies board identity and firmware version to the clipboard for bug reports."));
    m_copySupportInfoButton->setFixedHeight(Style::kButtonH);
    m_copySupportInfoButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    outerLayout->addWidget(m_copySupportInfoButton, 0, Qt::AlignLeft);
    outerLayout->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_sampleRateRx1Combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioInfoTab::onSampleRateChanged);

    if (m_model) {
        connect(m_model, &RadioModel::wireSampleRateChanged,
                this, &RadioInfoTab::onWireSampleRateChanged);
    }

    connect(m_copySupportInfoButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_currentInfo);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void RadioInfoTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    HPSDRModel model = HPSDRModel::HERMES;
    if (m_model) {
        model = m_model->hardwareProfile().model;
    }
    // Fill read-only identity labels. R-R3-49 / R-IOS-18: the same text the
    // Core's Setup description publishes (radioInfoFacts).
    const RadioInfoFacts facts = radioInfoFacts(info, caps, model);
    m_boardLabel->setText(facts.board);
    m_protocolLabel->setText(facts.protocol);
    m_adcCountLabel->setText(facts.adcCount);
    m_maxRxLabel->setText(facts.maxRx);
    m_firmwareLabel->setText(facts.firmware);
    m_macLabel->setText(facts.mac);
    m_ipLabel->setText(facts.ip);

    // Parity ruling C4: a remote window on an older Core changes only its
    // own receivers' rate; the rate box says so.
    m_sampleRateRx1Combo->setToolTip(
        m_model && !m_model->radioSampleRateReachesEveryReceiver()
            ? IStationLink::radioSampleRateUnavailableReason()
            : QString());

    // Rebuild RX1 combo from allowedSampleRates(proto, caps, model) — matches
    // Thetis setup.cs:847-852 filtering (per-protocol list ∩ caps.sampleRates,
    // with the RedPitaya extra-384k exception). Default selection is 192000
    // per setup.cs:866; if absent, first allowed entry.
    const auto allowed = allowedSampleRates(info.protocol, caps, model);
    const int fallbackRate = defaultSampleRate(info.protocol, caps, model);
    {
        QSignalBlocker blocker(m_sampleRateRx1Combo);
        m_sampleRateRx1Combo->clear();
        for (int rate : allowed) {
            m_sampleRateRx1Combo->addItem(QStringLiteral("%1").arg(rate), rate);
        }
        // Default selection — 192000 (setup.cs:866).
        int idx = -1;
        for (int i = 0; i < m_sampleRateRx1Combo->count(); ++i) {
            if (m_sampleRateRx1Combo->itemData(i).toInt() == fallbackRate) {
                idx = i;
                break;
            }
        }
        if (idx < 0 && m_sampleRateRx1Combo->count() > 0) {
            idx = 0;
        }
        if (idx >= 0) {
            m_sampleRateRx1Combo->setCurrentIndex(idx);
        }
    }

    // ANAN-8000DLE volts/amps toggle: visible only for ANAN8000D model.
    // HPSDRModel::ANAN8000D (value 10) is the OrionMKII-family SKU that
    // Thetis ships as the "ANAN-8000DLE"; gated here to avoid showing this
    // option on 7000DLE / AnvelinaPro3 (same OrionMKII board, different label).
    if (m_anan8000DleVoltsAmpsToggle) {
        const bool is8000D = (model == HPSDRModel::ANAN8000D);
        m_anan8000DleVoltsAmpsToggle->setVisible(is8000D);
    }

    // Build clipboard text for Copy Support Info button (the facts' own
    // text, which the Setup description's copy action carries too).
    m_currentInfo = facts.supportText();
}

// ── private slots ─────────────────────────────────────────────────────────────

void RadioInfoTab::onSampleRateChanged(int index)
{
    if (index < 0) { return; }
    int rate = m_sampleRateRx1Combo->itemData(index).toInt();
    if (rate > 0) {
        emit settingChanged(QStringLiteral("radioInfo/sampleRate"), rate);
        // Apply live via the RadioModel coordinator (Task 1.6) when a
        // radio is connected.  Returns >= 0 ms on success — the banner
        // then hides itself once wireSampleRateChanged fires from the
        // updated connection — or -1 when no active connection / WDSP
        // not ready (banner stays up; setting still persists for next
        // connect via settingChanged above).  Without this call the
        // live-apply infrastructure added in PR #219 (Task 1.6) was
        // unreachable from the UI; codex post-merge review flagged as P2.
        //
        // R-R3-46: a remote window's radio is the Core's. The rate saved
        // above stays the Core's default for that radio (its next connect).
        // Parity ruling C4: every window makes the same change now, without
        // a reconnect: every receiver and the radio's own rate
        // (RadioModel::requestRadioSampleRate; a remote window asks the
        // Core, whose confirm step asks another device first). R-R3-39: a
        // local change runs on the receive lane; this returns at once, and
        // wireSampleRateChanged hides the banner when it is done.
        if (m_model) {
            m_model->requestRadioSampleRate(rate);
        }
        updateReconnectBanner();
    }
}

void RadioInfoTab::onWireSampleRateChanged(double hz)
{
    m_activeWireRate = static_cast<int>(hz);
    updateReconnectBanner();
}

void RadioInfoTab::updateReconnectBanner()
{
    // Banner shows only when a radio is connected AND the combo's selected
    // rate differs from the active wire rate. m_activeWireRate is 0 when
    // no radio has ever connected this session — hide the banner then.
    if (!m_model || !m_model->isConnected() || m_activeWireRate <= 0) {
        m_reconnectBanner->setVisible(false);
        return;
    }
    const int selected = m_sampleRateRx1Combo->currentData().toInt();
    if (selected <= 0 || selected == m_activeWireRate) {
        m_reconnectBanner->setVisible(false);
        return;
    }
    m_reconnectBannerLabel->setText(
        tr("⚠ Reconnect to apply new sample rate (pending: %1 kHz, active: %2 kHz)")
            .arg(selected / 1000)
            .arg(m_activeWireRate / 1000));
    m_reconnectBanner->setVisible(true);
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void RadioInfoTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // sampleRate — match combo item data
    auto srIt = settings.constFind(QStringLiteral("sampleRate"));
    if (srIt != settings.constEnd()) {
        const int rate = srIt.value().toInt();
        QSignalBlocker blocker(m_sampleRateRx1Combo);
        for (int i = 0; i < m_sampleRateRx1Combo->count(); ++i) {
            if (m_sampleRateRx1Combo->itemData(i).toInt() == rate) {
                m_sampleRateRx1Combo->setCurrentIndex(i);
                break;
            }
        }
    }

    // activeRxCount widget removed from UI; persisted value (if any)
    // is honored by RadioModel via the connection-init path.
}

} // namespace NereusSDR
