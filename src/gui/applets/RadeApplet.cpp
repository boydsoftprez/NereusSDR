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

#include "RadeApplet.h"

#include "core/MicProfileManager.h"
#include "core/RadeChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
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
    syncFromModel();
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

    // Reset button enabled iff the active slice has a RadeChannel.
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
    if (!m_model) {
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
