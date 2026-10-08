// no-port-check: NereusSDR-original UI file. AppSettings key names and
// defaults cross-referenced against design doc Section 2.7 and the
// TciProtocol.h inventory. No Thetis code is translated here.

// =================================================================
// src/gui/setup/AudioTciPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original TCI section of Setup -> Audio -> Digital modes.
// See AudioTciPage.h for the full header and design notes.
//
// Phase 24 Task 24.2 (2026-05-10): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-24: R-R3-49 by J.J. Boyd (KG4VCF), with AI-assisted
// implementation via Anthropic Claude Code. The Slice B rate control is
// removed; nothing read its setting.
//
// 2026-10-06 (R-SPK-21, R-SPK-22): J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code. The TCI section of Audio > Digital modes: one
// sentence that TCI audio is separate from the PC and radio speaker
// volumes replaces the "Master Mute Behavior" box, and the settings sit
// in an "Audio stream" group and a "Transmit" group. Every key is kept.
// =================================================================

#include "AudioTciPage.h"
#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

AudioTciPage::AudioTciPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
{
    Q_UNUSED(model);
    setObjectName(QStringLiteral("audioTciSection"));
    buildUI();
}

void AudioTciPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // R-SPK-21: a section of Digital modes, which owns the title and the
    // scroll area.
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(8);

    // R-SPK-22: in place of the "Master Mute Behavior" box.
    auto* separateNote = new QLabel(
        tr("TCI audio is separate from the PC and radio speaker volumes; muting "
           "either one does not mute TCI."), this);
    separateNote->setObjectName(QStringLiteral("tciSeparateNote"));
    separateNote->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    separateNote->setWordWrap(true);
    m_layout->addWidget(separateNote);

    auto* streamGroup = new QGroupBox(tr("Audio stream"), this);
    streamGroup->setObjectName(QStringLiteral("tciAudioStreamGroup"));
    streamGroup->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* streamForm = new QFormLayout(streamGroup);
    streamForm->setSpacing(6);
    buildSampleRateGroup(streamGroup, streamForm);
    buildFormatGroup(streamGroup, streamForm);
    m_layout->addWidget(streamGroup);

    auto* txGroup = new QGroupBox(tr("Transmit"), this);
    txGroup->setObjectName(QStringLiteral("tciTransmitGroup"));
    txGroup->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* txForm = new QFormLayout(txGroup);
    txForm->setSpacing(6);
    buildTxDirectionGroup(txGroup, txForm);
    m_layout->addWidget(txGroup);
}

// ---------------------------------------------------------------------------
// Group 1: Output Sample Rate per Slice
// AppSettings: TciSliceA_OutputSampleRate (default 48000). The Slice B
// rate control is removed (R-R3-49); a saved TciSliceB_OutputSampleRate
// stays in the settings file.
// Slices C/D not exposed via TCI in Phase 3J-1 per design doc Section 1.2.
// ---------------------------------------------------------------------------
void AudioTciPage::buildSampleRateGroup(QGroupBox* group, QFormLayout* form)
{
    auto& s = AppSettings::instance();

    static const char* kRates[] = {
        "24000", "48000", "96000", "192000", nullptr
    };

    // Slice A
    m_sliceARateCombo = new QComboBox(group);
    m_sliceARateCombo->setObjectName(QStringLiteral("tciSliceARateCombo"));
    m_sliceARateCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    for (int i = 0; kRates[i] != nullptr; ++i) {
        m_sliceARateCombo->addItem(QString::fromLatin1(kRates[i]));
    }
    m_sliceARateCombo->setToolTip(
        tr("Output sample rate for the Slice A TCI audio stream (24000/48000/96000/192000 Hz). "
           "Higher rates require more CPU and network bandwidth."));
    {
        const QString saved = s.value(
            QStringLiteral("TciSliceA_OutputSampleRate"),
            QStringLiteral("48000")).toString();
        const int idx = m_sliceARateCombo->findText(saved);
        m_sliceARateCombo->setCurrentIndex(idx >= 0 ? idx : m_sliceARateCombo->findText(QStringLiteral("48000")));
    }
    connect(m_sliceARateCombo, &QComboBox::currentTextChanged, this, [](const QString& text) {
        AppSettings::instance().setValue(QStringLiteral("TciSliceA_OutputSampleRate"), text);
    });
    form->addRow(tr("Slice A rate:"), m_sliceARateCombo);

    // Slices C/D not exposed: informational note
    auto* noteLabel = new QLabel(
        // R-R3-17: user words (TCI carries Slices A and B only, Phase 3J-1).
        tr("Slices C and D are not available over TCI."), group);
    noteLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    form->addRow(noteLabel);
}

// ---------------------------------------------------------------------------
// Group 2: Sample Format
// AppSettings: TciAudioStreamSampleType (default "Float32"),
//              TciAudioStreamChannels (default 2),
//              TciAudioStreamSamples (default 2048, also on TCI Server page).
// ---------------------------------------------------------------------------
void AudioTciPage::buildFormatGroup(QGroupBox* group, QFormLayout* form)
{
    auto& s = AppSettings::instance();

    // Sample type
    m_formatCombo = new QComboBox(group);
    m_formatCombo->setObjectName(QStringLiteral("tciStreamFormatCombo"));
    m_formatCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_formatCombo->addItems({
        QStringLiteral("Int16"),
        QStringLiteral("Int24"),
        QStringLiteral("Int32"),
        QStringLiteral("Float32"),
    });
    m_formatCombo->setToolTip(
        tr("PCM sample format for TCI audio streams. Float32 is the most compatible; "
           "Int16 uses less bandwidth. The TCI client must support the chosen format."));
    {
        const QString saved = s.value(
            QStringLiteral("TciAudioStreamSampleType"),
            QStringLiteral("Float32")).toString();
        const int idx = m_formatCombo->findText(saved);
        m_formatCombo->setCurrentIndex(idx >= 0 ? idx : m_formatCombo->findText(QStringLiteral("Float32")));
    }
    connect(m_formatCombo, &QComboBox::currentTextChanged, this, [](const QString& text) {
        AppSettings::instance().setValue(QStringLiteral("TciAudioStreamSampleType"), text);
    });
    form->addRow(tr("Format:"), m_formatCombo);

    // Channel count
    m_channelsCombo = new QComboBox(group);
    m_channelsCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_channelsCombo->setObjectName(QStringLiteral("tciStreamChannelsCombo"));
    m_channelsCombo->addItem(tr("Mono"),   1);
    m_channelsCombo->addItem(tr("Stereo"), 2);
    // Each app starts with this count; Thetis starts it at 2 and a mono
    // stream carries the left channel (TCIServer.cs:781, 5897-5900
    // [v2.10.3.15]). The server reads it when an app connects.
    m_channelsCombo->setToolTip(
        tr("Number of audio channels a TCI app starts with. "
           "Stereo carries left and right; Mono carries the left channel only. "
           "An app can choose its own."));
    {
        const int saved = s.value(QStringLiteral("TciAudioStreamChannels"), 2).toInt();
        const int idx   = m_channelsCombo->findData(saved);
        m_channelsCombo->setCurrentIndex(idx >= 0 ? idx : m_channelsCombo->findData(2));
    }
    connect(m_channelsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int /*idx*/) {
        AppSettings::instance().setValue(
            QStringLiteral("TciAudioStreamChannels"),
            m_channelsCombo->currentData().toInt());
    });
    form->addRow(tr("Channels:"), m_channelsCombo);

    // Block size (shared key with CatTciServerPage Group 4)
    m_blockSizeSpin = new QSpinBox(group);
    m_blockSizeSpin->setObjectName(QStringLiteral("tciBlockSizeSpin"));
    m_blockSizeSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_blockSizeSpin->setRange(100, 2048);
    m_blockSizeSpin->setSuffix(tr(" samples"));
    m_blockSizeSpin->setToolTip(
        tr("Number of audio samples per TCI audio stream block (100-2048). "
           "Larger blocks reduce overhead but increase latency. "
           "This key is shared with Setup > Network > TCI Server."));
    m_blockSizeSpin->setValue(
        s.value(QStringLiteral("TciAudioStreamSamples"), 2048).toInt());
    connect(m_blockSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciAudioStreamSamples"), v);
    });
    form->addRow(tr("Block size:"), m_blockSizeSpin);
}

// ---------------------------------------------------------------------------
// Group 3: TX Direction
// AppSettings: TciTxChannel (default "Both", shared with CatTciServerPage),
//              TciTxStreamBufferingMs (default 50, range 10..200).
// ---------------------------------------------------------------------------
void AudioTciPage::buildTxDirectionGroup(QGroupBox* group, QFormLayout* form)
{
    auto& s = AppSettings::instance();

    // TX channel (shared key with CatTciServerPage Group 4)
    m_txChannelCombo = new QComboBox(group);
    m_txChannelCombo->setObjectName(QStringLiteral("tciTxChannelCombo"));
    m_txChannelCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_txChannelCombo->addItems({
        QStringLiteral("Left"),
        QStringLiteral("Right"),
        QStringLiteral("Both"),
    });
    m_txChannelCombo->setToolTip(
        tr("Which audio channel carries the TX audio in the TCI audio stream. "
           "\"Both\" sends the same mono signal to both left and right channels. "
           "This key is shared with Setup > Network > TCI Server."));
    {
        const QString saved = s.value(QStringLiteral("TciTxChannel"),
                                       QStringLiteral("Both")).toString();
        const int idx = m_txChannelCombo->findText(saved);
        m_txChannelCombo->setCurrentIndex(
            idx >= 0 ? idx : m_txChannelCombo->findText(QStringLiteral("Both")));
    }
    connect(m_txChannelCombo, &QComboBox::currentTextChanged, this, [](const QString& text) {
        AppSettings::instance().setValue(QStringLiteral("TciTxChannel"), text);
    });
    form->addRow(tr("TX channel:"), m_txChannelCombo);

    // TX stream buffering
    m_txBufferingSpin = new QSpinBox(group);
    m_txBufferingSpin->setObjectName(QStringLiteral("tciTxBufferingSpin"));
    m_txBufferingSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_txBufferingSpin->setRange(10, 200);
    m_txBufferingSpin->setSuffix(tr(" ms"));
    m_txBufferingSpin->setToolTip(
        tr("How much TX audio the TCI bridge buffers before forwarding to the transmit "
           "pipeline (10-200 ms). Lower values reduce TX latency at the cost of possible "
           "underruns if the TCI client cannot deliver audio fast enough."));
    m_txBufferingSpin->setValue(
        s.value(QStringLiteral("TciTxStreamBufferingMs"), 50).toInt());
    connect(m_txBufferingSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciTxStreamBufferingMs"), v);
    });
    form->addRow(tr("TX buffering:"), m_txBufferingSpin);
}

} // namespace NereusSDR
