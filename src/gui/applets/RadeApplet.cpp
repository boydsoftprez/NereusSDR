// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - RadeApplet implementation (Phase 3R Task L2).
//
// NereusSDR-native applet; see RadeApplet.h for full design notes.
//
// Modification history (NereusSDR):
//   2026-05-11 - Created for Phase 3R Task L2 by J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude
//                Code.
//   2026-09-23 - R-R3-21: the profile combo follows the negotiated
//                transmit permission; on a remote-station model Reset
//                vocoder is unavailable with the same reason and the
//                applet never looks up this window's own DSP. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): in a remote window the profile
//                combo picks the Core's profiles and Reset vocoder resets
//                the Core's RADE transmit vocoder (rade.resetVocoder),
//                both following setTxProfilePermitted. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-05 - Reset the Qt 6.11.0 Cocoa popup model before replacing
//                mirrored profiles, invalidating expired accessible cells.
//                J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include "RadeApplet.h"

#include "core/MicProfileManager.h"
#include "core/RadeChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/session/IStationLink.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QVariant>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

// RADE accent purple. Matches the L3 mode-menu / VFO chip colour for
// visual consistency across the RADE surface.
constexpr const char* kRadePurple = "#a78bfa";

// Threshold above which the codec is considered "good copy" (green).
// Below this, marginal (yellow). Same threshold as VfoWidget L1.
constexpr double kGoodSnrDb = 5.0;

// Format a numeric SNR as "+N dB" / "-N dB" (integer-rounded, single sign).
// NaN -> placeholder dashes.
QString formatSnr(double db)
{
    if (qIsNaN(db)) {
        return QStringLiteral(" -   - ");
    }
    return QString::asprintf("%+d dB", static_cast<int>(std::lround(db)));
}

QString snrColour(double db)
{
    if (qIsNaN(db)) {
        return QStringLiteral("#7a8088");
    }
    return (db < kGoodSnrDb) ? QStringLiteral("#e6c200")
                             : QStringLiteral("#4caf50");
}

}  // namespace

RadeApplet::RadeApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    buildUI();
    wireSignals();
    // R-R3-21: a remote-station model starts with transmit denied until
    // the station grants it, as the TX applet and the VFO flag do.
    if (isRemoteModel()) {
        setTransmitPermitted(false);
        setTxProfilePermitted(false);
    }
    syncFromModel();
}

bool RadeApplet::isRemoteModel() const
{
    return m_model && !m_model->ownsLocalDsp();
}

void RadeApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

    auto* body = new QWidget(this);
    body->setStyleSheet(
        QStringLiteral("QWidget { background: #0a0a18; color: #c8d8e8; }"));
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(8, 8, 8, 8);
    bodyLayout->setSpacing(6);

    // Row 1: profile combo
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* lbl = new QLabel(QStringLiteral("Profile"), body);
        lbl->setStyleSheet(QStringLiteral("color: #8aa8c0; font-size: 11px;"));
        lbl->setFixedWidth(62);
        m_profileCombo = new QComboBox(body);
        m_profileCombo->setStyleSheet(QStringLiteral(
            "QComboBox { background: #1a2a3a; color: #c8d8e8; "
            "border: 1px solid #304050; border-radius: 2px; "
            "padding: 2px 4px; font-size: 11px; }"));
        row->addWidget(lbl);
        row->addWidget(m_profileCombo, 1);
        bodyLayout->addLayout(row);
    }

    // Row 2: sync indicator + SNR readout
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);

        m_syncIndicator = new QLabel(body);
        m_syncIndicator->setFixedSize(12, 12);
        m_syncIndicator->setStyleSheet(QStringLiteral(
            "QLabel { background: #7a8088; border-radius: 6px; }"));

        auto* syncLbl = new QLabel(QStringLiteral("Sync"), body);
        syncLbl->setStyleSheet(QStringLiteral(
            "color: #8aa8c0; font-size: 11px;"));

        m_snrLabel = new QLabel(QStringLiteral(" -   - "), body);
        m_snrLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #7a8088; font-size: 11px; "
            "font-weight: bold; background: transparent; }"));
        m_snrLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row->addWidget(m_syncIndicator);
        row->addWidget(syncLbl);
        row->addStretch(1);
        row->addWidget(m_snrLabel);
        bodyLayout->addLayout(row);
    }

    // Row 3: freq offset readout
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* lbl = new QLabel(QStringLiteral("Offset"), body);
        lbl->setStyleSheet(QStringLiteral("color: #8aa8c0; font-size: 11px;"));
        lbl->setFixedWidth(62);
        m_freqOffsetLabel = new QLabel(QStringLiteral("0 Hz"), body);
        m_freqOffsetLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #c8d8e8; font-size: 11px; "
            "font-weight: bold; background: transparent; }"));
        m_freqOffsetLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(lbl);
        row->addStretch(1);
        row->addWidget(m_freqOffsetLabel);
        bodyLayout->addLayout(row);
    }

    // Row 4: last decoded
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* lbl = new QLabel(QStringLiteral("Last RX"), body);
        lbl->setStyleSheet(QStringLiteral("color: #8aa8c0; font-size: 11px;"));
        lbl->setFixedWidth(62);
        m_lastDecodedLabel = new QLabel(QStringLiteral("--"), body);
        m_lastDecodedLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; "
            "font-weight: bold; background: transparent; }")
                .arg(QString::fromLatin1(kRadePurple)));
        m_lastDecodedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_lastDecodedLabel->setMinimumWidth(120);
        row->addWidget(lbl);
        row->addStretch(1);
        row->addWidget(m_lastDecodedLabel);
        bodyLayout->addLayout(row);
    }

    // Row 5: reset vocoder button
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        m_resetButton = new QPushButton(QStringLiteral("Reset vocoder"), body);
        m_resetButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a2a3a; color: #c8d8e8; "
            "border: 1px solid #304050; border-radius: 2px; "
            "padding: 4px 10px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { border: 1px solid %1; }"
            "QPushButton:pressed { background: #2a3a4a; }"
            "QPushButton:disabled { background: #1a1a2a; "
            "color: #556070; border: 1px solid #2a3040; }")
                .arg(QString::fromLatin1(kRadePurple)));
        row->addStretch(1);
        row->addWidget(m_resetButton);
        row->addStretch(1);
        bodyLayout->addLayout(row);
    }

    root->addWidget(body);
}

void RadeApplet::wireSignals()
{
    if (!m_model) {
        return;
    }

    // Subscribe to the RadioModel-level signals (I5 plus the L2
    // freqOffset extension). The applet does NOT subscribe to the
    // per-channel RadeChannel signals directly so that a J3 channel
    // teardown/rebuild does not require any rewire here.
    connect(m_model, &RadioModel::radeSyncChanged,
            this,    &RadeApplet::onSyncChanged);
    connect(m_model, &RadioModel::radeSnrChanged,
            this,    &RadeApplet::onSnrChanged);
    connect(m_model, &RadioModel::radeFreqOffsetChanged,
            this,    &RadeApplet::onFreqOffsetChanged);

    // RxDecodeModel emits decodeAdded on every new decode (RADE +
    // future WSJT-X); the slot filters on source == "rade_text".
    if (auto* dec = m_model->rxDecodeModel()) {
        connect(dec, &RxDecodeModel::decodeAdded,
                this, &RadeApplet::onDecodeAdded);
    }

    // Profile combo: populate from MicProfileManager and wire activated.
    if (auto* mgr = m_model->micProfileManager()) {
        connect(m_profileCombo, &QComboBox::textActivated,
                this,           &RadeApplet::onProfileComboActivated);
        connect(mgr, &MicProfileManager::activeProfileChanged,
                this, &RadeApplet::onActiveProfileChanged);
        connect(mgr, &MicProfileManager::profileListChanged,
                this, [this]() { syncFromModel(); });
    }

    connect(m_resetButton, &QPushButton::clicked,
            this,          &RadeApplet::onResetVocoderClicked);
}

void RadeApplet::syncFromModel()
{
    if (!m_model || !m_profileCombo) {
        return;
    }
    auto* mgr = m_model->micProfileManager();
    if (!mgr) {
        return;
    }

    // Repopulate combo from the live profile list.
    QSignalBlocker block(m_profileCombo);
#if defined(Q_OS_MAC)
    if (QGuiApplication::platformName() == QStringLiteral("cocoa")
        && qVersion() == QStringLiteral("6.11.0")) {
        // As in DeviceCard, reset the actual model so persistent cell indexes
        // invalidate before accessibility clears its expired Cocoa child IDs
        // (Qt 6.11 qstandarditemmodel.cpp:2264-2275; itemviews.cpp:645-708).
        // These rows are deliberately replaced; no retained entries need cloning.
        if (auto* model = qobject_cast<QStandardItemModel*>(m_profileCombo->model())) {
            model->clear();
        }
    }
#endif
    m_profileCombo->clear();
    const QStringList names = mgr->profileNames();
    for (const QString& name : names) {
        m_profileCombo->addItem(name);
    }

    // Default to "RADE" preset if present; otherwise leave the combo
    // on the manager's active profile (avoids stomping a user choice
    // on first paint if the K1 preset has been deleted from settings).
    int idx = m_profileCombo->findText(QStringLiteral("RADE"));
    if (idx < 0) {
        idx = m_profileCombo->findText(mgr->activeProfileName());
    }
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }

    // Initial sync indicator state from the last cached values.
    repaintSyncIndicator();

    // Reset button enabled iff the active slice has a RadeChannel. A remote
    // window has no RADE channel of its own (the Core runs the vocoder), so
    // it does not look one up; updateTransmitControlAvailability() gives
    // the button its reason there.
    if (!isRemoteModel()) {
        SliceModel* slice = m_model->activeSlice();
        bool hasChannel = false;
        if (slice) {
            const int sliceIdx = slice->sliceIndex();
            if (auto* eng = m_model->wdspEngine()) {
                hasChannel = (eng->radeChannel(sliceIdx) != nullptr);
            }
        }
        m_resetButton->setEnabled(hasChannel);
    }
    updateTransmitControlAvailability();
}

void RadeApplet::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_transmitPermitted = permitted;
    m_transmitReason = reason.isEmpty()
        ? tr("Transmit controls are unavailable until the Core confirms "
             "transmit permission.")
        : reason;
    updateTransmitControlAvailability();
}

// R-R3-49 (parity Task 3): the profile combo and, in a remote window, Reset
// vocoder. Both key nothing; the Core takes them while its radio is off the
// air (transmitSettingsVersion 3).
void RadeApplet::setTxProfilePermitted(bool permitted, const QString& reason)
{
    m_txProfilePermitted = permitted;
    m_txProfileReason = reason.isEmpty()
        ? IStationLink::transmitSettingsUnavailableReason()
        : reason;
    updateTransmitControlAvailability();
}

void RadeApplet::updateTransmitControlAvailability()
{
    if (m_profileCombo) {
        static constexpr auto kSavedTooltip = "RadeSavedTransmitTooltip";
        static constexpr auto kSavedDescription = "RadeSavedTransmitDescription";
        static constexpr auto kSavedEnabled = "RadeSavedTransmitEnabled";
        if (!m_txProfilePermitted) {
            if (!m_profileCombo->property(kSavedTooltip).isValid()) {
                m_profileCombo->setProperty(kSavedTooltip, m_profileCombo->toolTip());
                m_profileCombo->setProperty(kSavedDescription,
                                            m_profileCombo->accessibleDescription());
                m_profileCombo->setProperty(kSavedEnabled, m_profileCombo->isEnabled());
            }
            m_profileCombo->setEnabled(false);
            m_profileCombo->setToolTip(m_txProfileReason);
            m_profileCombo->setAccessibleDescription(m_txProfileReason);
        } else if (m_profileCombo->property(kSavedTooltip).isValid()) {
            m_profileCombo->setEnabled(m_profileCombo->property(kSavedEnabled).toBool());
            m_profileCombo->setToolTip(m_profileCombo->property(kSavedTooltip).toString());
            m_profileCombo->setAccessibleDescription(
                m_profileCombo->property(kSavedDescription).toString());
            m_profileCombo->setProperty(kSavedTooltip, QVariant());
            m_profileCombo->setProperty(kSavedDescription, QVariant());
            m_profileCombo->setProperty(kSavedEnabled, QVariant());
        }
    }
    if (m_resetButton && isRemoteModel()) {
        // R-R3-49 (parity Task 3): the Core resets its own RADE transmit
        // vocoder (rade.resetVocoder) while it takes transmit settings and
        // its radio is off the air; otherwise the reason says why not.
        const QString reason = m_txProfilePermitted ? QString() : m_txProfileReason;
        m_resetButton->setEnabled(m_txProfilePermitted);
        m_resetButton->setToolTip(reason);
        m_resetButton->setAccessibleDescription(reason);
    }
}

void RadeApplet::onSyncChanged(int sliceId, bool synced)
{
    if (!m_model) {
        return;
    }
    SliceModel* slice = m_model->activeSlice();
    if (!slice || slice->sliceIndex() != sliceId) {
        return;
    }
    m_synced = synced;
    repaintSyncIndicator();
}

void RadeApplet::onSnrChanged(int sliceId, float snrDb)
{
    if (!m_model) {
        return;
    }
    SliceModel* slice = m_model->activeSlice();
    if (!slice || slice->sliceIndex() != sliceId) {
        return;
    }
    m_lastSnrDb = static_cast<double>(snrDb);
    if (m_snrLabel) {
        m_snrLabel->setText(formatSnr(m_lastSnrDb));
        m_snrLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; "
            "font-weight: bold; background: transparent; }")
                .arg(snrColour(m_lastSnrDb)));
    }
    repaintSyncIndicator();
}

void RadeApplet::onFreqOffsetChanged(int sliceId, float hz)
{
    if (!m_model) {
        return;
    }
    SliceModel* slice = m_model->activeSlice();
    if (!slice || slice->sliceIndex() != sliceId) {
        return;
    }
    if (m_freqOffsetLabel) {
        m_freqOffsetLabel->setText(QString::asprintf("%+.0f Hz",
                                                     static_cast<double>(hz)));
    }
}

void RadeApplet::onDecodeAdded(const RxDecode& decode)
{
    // Only show RADE source decodes; WSJT-X decodes belong elsewhere.
    if (decode.source != QStringLiteral("rade_text")) {
        return;
    }
    if (m_lastDecodedLabel) {
        // Prefer the full payload (which contains callsign + grid when
        // grid is available); fall back to bare callsign.
        const QString text = decode.payload.isEmpty()
                                 ? decode.callsign
                                 : decode.payload;
        m_lastDecodedLabel->setText(text);
    }
}

void RadeApplet::onResetVocoderClicked()
{
    if (!m_model) {
        return;
    }
    if (isRemoteModel()) {
        // R-R3-49 (parity Task 3): the Core's RADE channel, through the
        // Core; a refusal comes back as a notice.
        if (!m_txProfilePermitted) {
            return;
        }
        IStationLink* const link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link != nullptr
            ? link->requestRadeResetVocoder()
            : IStationLink::CommandOutcome{false,
                  IStationLink::transmitSettingsUnavailableReason()};
        if (!outcome.sent) {
            m_model->reportStationSliceCommandRejected(outcome.reason);
        }
        return;
    }
    SliceModel* slice = m_model->activeSlice();
    if (!slice) {
        return;
    }
    auto* eng = m_model->wdspEngine();
    if (!eng) {
        return;
    }
    if (RadeChannel* ch = eng->radeChannel(slice->sliceIndex())) {
        ch->resetTx();
    }
}

void RadeApplet::onActiveProfileChanged(const QString& name)
{
    if (!m_profileCombo) {
        return;
    }
    QSignalBlocker block(m_profileCombo);
    const int idx = m_profileCombo->findText(name);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }
}

void RadeApplet::onProfileComboActivated(const QString& name)
{
    // R-R3-21: the microphone profile is a transmit setting; R-R3-49
    // (parity Task 3): the Core takes it off the air.
    if (!m_model || !m_txProfilePermitted) {
        return;
    }
    auto* mgr = m_model->micProfileManager();
    if (!mgr) {
        return;
    }
    mgr->setActiveProfile(name, &m_model->transmitModel());
}

void RadeApplet::repaintSyncIndicator()
{
    if (!m_syncIndicator) {
        return;
    }
    // Decision matrix:
    //   not synced              -> dim grey (#7a8088)
    //   synced + snr < 5 dB    -> yellow   (#e6c200)
    //   synced + snr >= 5 dB   -> green    (#4caf50)
    //   synced + snr NaN        -> yellow (treated as marginal)
    QString colour;
    if (!m_synced) {
        colour = QStringLiteral("#7a8088");
    } else if (qIsNaN(m_lastSnrDb) || m_lastSnrDb < kGoodSnrDb) {
        colour = QStringLiteral("#e6c200");
    } else {
        colour = QStringLiteral("#4caf50");
    }
    m_syncIndicator->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border-radius: 6px; }").arg(colour));
}

}  // namespace NereusSDR
