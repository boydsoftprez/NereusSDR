// =================================================================
// src/gui/applets/VaxApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/DaxApplet.h
//   src/gui/DaxApplet.cpp
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Phase 3O Sub-Phase 9 Task 9.2b.
//                 See VaxApplet.h for full provenance / scope notes.
// =================================================================

#include "VaxApplet.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "gui/widgets/MeterSlider.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

namespace {

// Style constants match AetherSDR DaxApplet.cpp palette + STYLEGUIDE.md
// applet rows. kSectionStyle is applied at the applet root so the body
// inherits the transparent-row look; kGreenToggle / kDimLabel /
// kStatusLabel mirror DaxApplet.cpp:17-33 verbatim aside from the
// DAX→VAX text rename.
constexpr const char* kSectionStyle =
    "QWidget { background: transparent; }"
    "QLabel { color: #8090a0; font-size: 11px; }"
    "QPushButton { background: #1a2a3a; border: 1px solid #205070;"
    "  border-radius: 3px; padding: 2px 8px; font-size: 11px; font-weight: bold; color: #c8d8e8; }"
    "QPushButton:hover { background: #204060; }";

const QString kGreenToggle =
    "QPushButton { background: #1a2a3a; border: 1px solid #205070; border-radius: 3px;"
    " color: #c8d8e8; font-size: 11px; font-weight: bold; padding: 2px 8px; }"
    "QPushButton:hover { background: #204060; }"
    "QPushButton:checked { background: #006040; color: #00ff88; border: 1px solid #00a060; }";

constexpr const char* kDimLabel =
    "QLabel { color: #8090a0; font-size: 11px; }";

const QString kStatusLabel = "QLabel { color: #506070; font-size: 11px; }";

constexpr const char* kDeviceLabel =
    "QLabel { color: #506070; font-size: 10px; }";

// Slice letters A..H by sliceId — matches AetherSDR DaxApplet.cpp:159.
// NereusSDR tops out at 8 slices (same as AetherSDR) so a static 8-char
// array suffices.
constexpr char kSliceLetters[] = "ABCDEFGH";

// AppSettings key helpers. Keys are PascalCase per spec §5.4.
QString kRxGainKey(int ch)    { return QStringLiteral("audio/Vax%1/RxGain").arg(ch); }
QString kMutedKey(int ch)     { return QStringLiteral("audio/Vax%1/Muted").arg(ch); }
#ifdef Q_OS_WIN
// Only consumed inside the Windows device-label fallback; guard to avoid
// -Wunused-function on macOS/Linux.
QString kDeviceKey(int ch)    { return QStringLiteral("audio/Vax%1/DeviceName").arg(ch); }
#endif
QString kTxGainKey()          { return QStringLiteral("audio/TxGain"); }

} // namespace

VaxApplet::VaxApplet(RadioModel* model, AudioEngine* audio, QWidget* parent)
    : AppletWidget(model, parent)
    , m_audio(audio)
{
    buildUi();
    connectSliceTagsTracking();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void VaxApplet::buildUi()
{
    setStyleSheet(QString::fromLatin1(kSectionStyle));

    auto& settings = AppSettings::instance();

    // Outer layout — zero margins so the title bar (added by the applet
    // panel wrapper) sits flush to the edges. Body is padded separately.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);
    outer->addWidget(body);

    // ── RX channel rows (VAX 1..4) ────────────────────────────────────
    for (int i = 0; i < kChannels; ++i) {
        const int channel = i + 1;

        // Row 1: [VAX N:] [Slice tag] [MeterSlider] [Mute]
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);

        auto* chLabel = new QLabel(QStringLiteral("VAX %1:").arg(channel), body);
        chLabel->setStyleSheet(QString::fromLatin1(kDimLabel));
        chLabel->setFixedWidth(44);
        row->addWidget(chLabel);

        m_tagsLbl[i] = new QLabel(QStringLiteral("\u2014"), body);
        m_tagsLbl[i]->setStyleSheet(kStatusLabel);
        m_tagsLbl[i]->setFixedWidth(56);
        row->addWidget(m_tagsLbl[i]);

        m_rxMeter[i] = new MeterSlider(body);
        // Apply saved RX gain BEFORE wiring signals so we don't
        // immediately ping-pong a spurious setting back into AppSettings.
        {
            const float saved = std::clamp(
                settings.value(kRxGainKey(channel), QStringLiteral("1.000"))
                    .toString().toFloat(),
                0.0f, 1.0f);
            QSignalBlocker b(m_rxMeter[i]);
            m_rxMeter[i]->setGain(saved);
            if (m_audio) {
                m_audio->setVaxRxGain(channel, saved);
            }
        }
        connect(m_rxMeter[i], &MeterSlider::gainChanged, this,
                [this, channel](float g) {
            if (m_audio) {
                m_audio->setVaxRxGain(channel, g);
            }
            auto& ss = AppSettings::instance();
            ss.setValue(kRxGainKey(channel), QString::number(g, 'f', 3));
            ss.save();
        });
        row->addWidget(m_rxMeter[i], 1);

        m_muteBtn[i] = new QPushButton(QStringLiteral("Mute"), body);
        m_muteBtn[i]->setCheckable(true);
        m_muteBtn[i]->setStyleSheet(kGreenToggle);
        m_muteBtn[i]->setFixedSize(46, 20);
        m_muteBtn[i]->setToolTip(
            QStringLiteral("Mute VAX channel %1 (suppresses the tap without"
                           " affecting speakers)").arg(channel));
        {
            const bool savedMuted =
                settings.value(kMutedKey(channel), QStringLiteral("False"))
                    .toString() == QStringLiteral("True");
            QSignalBlocker b(m_muteBtn[i]);
            m_muteBtn[i]->setChecked(savedMuted);
            if (m_audio) {
                m_audio->setVaxMuted(channel, savedMuted);
            }
        }
        connect(m_muteBtn[i], &QPushButton::toggled, this,
                [this, channel](bool on) {
            if (m_audio) {
                m_audio->setVaxMuted(channel, on);
            }
            auto& ss = AppSettings::instance();
            ss.setValue(kMutedKey(channel), on ? QStringLiteral("True")
                                               : QStringLiteral("False"));
            ss.save();
        });
        row->addWidget(m_muteBtn[i]);

        vbox->addLayout(row);

        // Row 2: device label (read-only)
        auto* devRow = new QHBoxLayout;
        devRow->setContentsMargins(48, 0, 0, 0);
        devRow->setSpacing(0);
        m_deviceLbl[i] = new QLabel(deviceLabelFor(channel), body);
        m_deviceLbl[i]->setStyleSheet(QString::fromLatin1(kDeviceLabel));
        devRow->addWidget(m_deviceLbl[i]);
        devRow->addStretch();
        vbox->addLayout(devRow);
    }

    // Divider between VAX RX rows and the TX row.
    {
        auto* line = new QFrame(body);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(QStringLiteral("QFrame { color: #203040; }"));
        vbox->addWidget(line);
    }

    // ── TX row ────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);

        auto* txLabel = new QLabel(QStringLiteral("TX:"), body);
        txLabel->setStyleSheet(QString::fromLatin1(kDimLabel));
        txLabel->setFixedWidth(44);
        row->addWidget(txLabel);

        m_txTagsLbl = new QLabel(QStringLiteral("\u2014"), body);
        m_txTagsLbl->setStyleSheet(kStatusLabel);
        m_txTagsLbl->setFixedWidth(56);
        row->addWidget(m_txTagsLbl);

        m_txMeter = new MeterSlider(body);
        {
            const float saved = std::clamp(
                settings.value(kTxGainKey(), QStringLiteral("1.000"))
                    .toString().toFloat(),
                0.0f, 1.0f);
            QSignalBlocker b(m_txMeter);
            m_txMeter->setGain(saved);
            if (m_audio) {
                m_audio->setVaxTxGain(saved);
            }
        }
        connect(m_txMeter, &MeterSlider::gainChanged, this, [this](float g) {
            if (m_audio) {
                m_audio->setVaxTxGain(g);
            }
            auto& ss = AppSettings::instance();
            ss.setValue(kTxGainKey(), QString::number(g, 'f', 3));
            ss.save();
        });
        row->addWidget(m_txMeter, 1);

        // Spacer matching the RX rows' Mute-button footprint so the
        // MeterSlider endpoints line up across the TX/RX divider.
        auto* spacer = new QWidget(body);
        spacer->setFixedWidth(46);
        row->addWidget(spacer);

        vbox->addLayout(row);
    }

    // ── Reverse channel: AudioEngine → widget (echo-prevented) ────────
    if (m_audio) {
        connect(m_audio, &AudioEngine::vaxRxGainChanged, this,
                [this](int channel, float gain) {
            if (channel < 1 || channel > kChannels || !m_rxMeter[channel - 1]) {
                return;
            }
            QSignalBlocker b(m_rxMeter[channel - 1]);
            m_rxMeter[channel - 1]->setGain(gain);
        });
        connect(m_audio, &AudioEngine::vaxMutedChanged, this,
                [this](int channel, bool muted) {
            if (channel < 1 || channel > kChannels || !m_muteBtn[channel - 1]) {
                return;
            }
            QSignalBlocker b(m_muteBtn[channel - 1]);
            m_muteBtn[channel - 1]->setChecked(muted);
        });
        connect(m_audio, &AudioEngine::vaxTxGainChanged, this,
                [this](float gain) {
            if (!m_txMeter) {
                return;
            }
            QSignalBlocker b(m_txMeter);
            m_txMeter->setGain(gain);
        });
    }

    // ── 20 Hz level poll (Pattern A — isolated per-applet timer) ──────
    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(50);  // 50 ms = 20 Hz, "quiet meter" feel
    connect(m_levelTimer, &QTimer::timeout, this, &VaxApplet::pollLevels);
}

void VaxApplet::connectSliceTagsTracking()
{
    if (!m_model) {
        return;
    }

    // Wire new slices as they come online. RadioModel::sliceAdded carries
    // the new slice's index; dereference through slices() to get the pointer.
    // Differs from AetherSDR DaxApplet.cpp:150 (which receives the SliceModel*
    // directly) because NereusSDR's RadioModel::sliceAdded(int) surface
    // (confirmed in RadioModel.h) matches the NereusSDR signal convention.
    //
    // Lifetime: connections to individual SliceModel instances via
    // Qt::AutoConnection are auto-severed when the SliceModel is destroyed
    // (QObject destructor semantics), so no explicit bookkeeping is needed
    // on slice removal.
    connect(m_model, &RadioModel::sliceAdded, this, [this](int index) {
        SliceModel* s = m_model->sliceAt(index);
        if (!s) {
            return;
        }
        connect(s, &SliceModel::vaxChannelChanged, this,
                [this]() { updateTagsLabels(); });
        connect(s, &SliceModel::txSliceChanged, this,
                [this]() { updateTagsLabels(); });
        updateTagsLabels();
    });

    // Slice removal drops all per-slice signals (QObject auto-disconnect),
    // but the tag view still references the old sliceIndex letters until a
    // subsequent vaxChannelChanged / txSliceChanged fires elsewhere. Refresh
    // on removal so deleted slices clear out of the tag row immediately.
    connect(m_model, &RadioModel::sliceRemoved, this,
            [this](int) { updateTagsLabels(); });

    // Also scan existing slices so tags populate correctly when the
    // applet is constructed after slices already exist (e.g. restored
    // from persistence on app start).
    for (SliceModel* s : m_model->slices()) {
        if (!s) {
            continue;
        }
        connect(s, &SliceModel::vaxChannelChanged, this,
                [this]() { updateTagsLabels(); });
        connect(s, &SliceModel::txSliceChanged, this,
                [this]() { updateTagsLabels(); });
    }
    updateTagsLabels();
}

void VaxApplet::updateTagsLabels()
{
    // Reset every RX tag to the em-dash placeholder, then fill in.
    for (int i = 0; i < kChannels; ++i) {
        if (m_tagsLbl[i]) {
            m_tagsLbl[i]->setText(QStringLiteral("\u2014"));
        }
    }
    if (m_txTagsLbl) {
        m_txTagsLbl->setText(QStringLiteral("\u2014"));
    }
    if (!m_model) {
        return;
    }
    constexpr int kMaxLetterIndex =
        static_cast<int>(sizeof(kSliceLetters)) - 1;  // trailing NUL
    for (SliceModel* s : m_model->slices()) {
        if (!s) {
            continue;
        }
        // Use sliceIndex() as the letter source; AetherSDR uses sliceId()
        // which is the same concept in NereusSDR (0-based per-radio index).
        const int idx = s->sliceIndex();
        if (idx < 0 || idx >= kMaxLetterIndex) {
            continue;
        }
        const QChar letter = QChar::fromLatin1(kSliceLetters[idx]);

        const int ch = s->vaxChannel();
        if (ch >= 1 && ch <= kChannels && m_tagsLbl[ch - 1]) {
            // If multiple slices share a channel, append letters with "+".
            const QString existing = m_tagsLbl[ch - 1]->text();
            if (existing == QStringLiteral("\u2014")) {
                m_tagsLbl[ch - 1]->setText(QStringLiteral("Slice %1").arg(letter));
            } else {
                m_tagsLbl[ch - 1]->setText(existing + QStringLiteral("+") + letter);
            }
        }
        if (s->isTxSlice() && m_txTagsLbl) {
            m_txTagsLbl->setText(QStringLiteral("Slice %1").arg(letter));
        }
    }
}

void VaxApplet::pollLevels()
{
    if (!m_audio) {
        return;
    }
    for (int i = 0; i < kChannels; ++i) {
        if (!m_rxMeter[i]) {
            continue;
        }
        m_rxMeter[i]->setLevel(m_audio->vaxRxLevel(i + 1));
    }
    if (m_txMeter) {
        m_txMeter->setLevel(m_audio->vaxTxLevel());
    }
}

QString VaxApplet::deviceLabelFor(int channel) const
{
#ifdef Q_OS_WIN
    // TODO(sub-phase-12-windows-device-pick): Setup → Audio page wires
    // audio/Vax<ch>/DeviceName to a real device picker. Until then,
    // display whatever's persisted (typically blank on fresh installs).
    const QString name = AppSettings::instance()
        .value(QStringLiteral("audio/Vax%1/DeviceName").arg(channel))
        .toString();
    if (name.isEmpty()) {
        return QStringLiteral("(no device)");
    }
    return name;
#else
    // macOS CoreAudioHalBus + Linux LinuxPipeBus register the virtual
    // device under this exact name — see AudioEngine::makeVaxBus.
    return QStringLiteral("NereusSDR VAX %1").arg(channel);
#endif
}

void VaxApplet::syncFromModel()
{
    // Reverse-signal wiring already mirrors AudioEngine state into the
    // widgets live; pull tags + device labels back to fresh defaults in
    // case AppSettings (or the slice layout) changed while we were
    // hidden.
    updateTagsLabels();
    for (int i = 0; i < kChannels; ++i) {
        if (m_deviceLbl[i]) {
            m_deviceLbl[i]->setText(deviceLabelFor(i + 1));
        }
    }
}

void VaxApplet::showEvent(QShowEvent* e)
{
    AppletWidget::showEvent(e);
    if (m_levelTimer) {
        m_levelTimer->start();
    }
}

void VaxApplet::hideEvent(QHideEvent* e)
{
    if (m_levelTimer) {
        m_levelTimer->stop();
    }
    AppletWidget::hideEvent(e);
}

} // namespace NereusSDR
