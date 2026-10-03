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
//   2026-09-23 - R-R3-21: unavailable, with a plain reason, on a
//                 remote-station model. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23 - R-R3-44: usable in a remote window again. The VAX
//                 channels there are this computer's, fed by the Core's
//                 receiver streams (RemoteVaxRouter), so the gain, mute and
//                 level rows work; the TX row follows the transmit
//                 permission (setTransmitPermitted). J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-09-28 - iPhone app plan Task 25 (R-IOS-18): the "Station computer"
//                 section below this computer's channels, the Core
//                 computer's VAX through the Core's `vax` object; its TX row
//                 follows this device's transmit permission, and its meters
//                 are wanted only while it is shown and the applet visible.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-30 - The section is titled "Core computer", and where it does
//                 not apply (a local window, or a Core that shares no VAX
//                 channels) it stays in place disabled, with a plain reason
//                 on each of its controls, instead of hidden. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 - JJ's ruling: a window that runs the radio directly hides
//                 the section (it can never have one); a remote window
//                 keeps it, disabled with its reason while the Core shares
//                 no VAX. Its labels grey when it is disabled. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include "VaxApplet.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/session/StationVaxFacade.h"
#include "gui/StyleConstants.h"
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
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

namespace {

// §A2 exception — vaxSectionStyle(): root-widget stylesheet sets a
// transparent background and generic QPushButton/QLabel defaults for all
// child widgets. Colors use canonical palette constants, but font-size
// (11px) and button padding (2px 8px) differ from Style::buttonBaseStyle()
// (10px / 2px 4px), so a file-local helper is required per §A2 rules.
static inline QString vaxSectionStyle()
{
    return QStringLiteral(
        "QWidget { background: transparent; }"
        "QLabel { color: %1; font-size: 11px; }"
        "QPushButton { background: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 2px 8px; font-size: 11px; font-weight: bold; color: %4; }"
        "QPushButton:hover { background: %5; }"
    ).arg(Style::kTextSecondary, Style::kButtonBg, Style::kBorder,
          Style::kTextPrimary, Style::kButtonAltHover);
}

// §A2 exception — vaxButtonStyle(): Mute toggle base style. Uses 11px
// font-size and 2px 8px padding (matching original AetherSDR DaxApplet.cpp),
// versus Style::buttonBaseStyle() which uses 10px / 2px 4px. A file-local
// helper is required to avoid a visible size regression. Combine with
// Style::greenCheckedStyle() at call sites.
static inline QString vaxButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
        " color: %3; font-size: 11px; font-weight: bold; padding: 2px 8px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(Style::kButtonBg, Style::kBorder, Style::kTextPrimary,
          Style::kButtonAltHover);
}

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

    // R-R3-44: in a remote window the VAX channels are this computer's,
    // fed by the Core's receiver streams through RemoteVaxRouter, which
    // applies this applet's gain and mute (AudioEngine::writeVaxOutput). The
    // TX row sets the level of VAX used as the microphone, which waits for
    // remote transmit: it starts unavailable until MainWindow pushes the
    // negotiated permission.
    if (m_model && !m_model->ownsLocalDsp()) {
        setTransmitPermitted(false, QString());
    }
}

namespace {

// The tooltip a widget had before a reason replaced it, kept on the widget.
constexpr auto kSavedStationTooltip = "VaxSavedStationTooltip";

// While the Core computer section cannot be used, each of its controls
// shows the reason in place of its own tooltip; the tooltip comes back
// when the section can be used again (the TX row's pattern below).
// A label in the Core computer section: its own color, and the disabled
// text color while the section is disabled, as the title has.
QString stationLabelStyle(const char* color, int pixelSize)
{
    return QStringLiteral("QLabel { color: %1; font-size: %2px; }"
                          "QLabel:disabled { color: %3; }")
        .arg(QLatin1String(color))
        .arg(pixelSize)
        .arg(QLatin1String(Style::kDisabledText));
}

void showStationReason(QWidget* w, bool available, const QString& reason)
{
    if (w == nullptr) {
        return;
    }
    if (!available) {
        if (!w->property(kSavedStationTooltip).isValid()) {
            w->setProperty(kSavedStationTooltip, w->toolTip());
        }
        w->setToolTip(reason);
        return;
    }
    if (w->property(kSavedStationTooltip).isValid()) {
        w->setToolTip(w->property(kSavedStationTooltip).toString());
        w->setProperty(kSavedStationTooltip, QVariant());
    }
}

} // namespace

void VaxApplet::setTransmitPermitted(bool permitted, const QString& reason)
{
    if (!m_txMeter) {
        return;
    }
    static constexpr auto kSavedTooltip = "VaxSavedTransmitTooltip";
    if (!permitted) {
        const QString shown = reason.isEmpty()
            ? tr("Transmit controls are unavailable until the Core confirms transmit permission.")
            : reason;
        if (!m_txMeter->property(kSavedTooltip).isValid()) {
            m_txMeter->setProperty(kSavedTooltip, m_txMeter->toolTip());
        }
        m_txMeter->setEnabled(false);
        m_txMeter->setToolTip(shown);
        return;
    }
    m_txMeter->setEnabled(true);
    if (m_txMeter->property(kSavedTooltip).isValid()) {
        m_txMeter->setToolTip(m_txMeter->property(kSavedTooltip).toString());
        m_txMeter->setProperty(kSavedTooltip, QVariant());
    }
}

void VaxApplet::buildUi()
{
    setStyleSheet(vaxSectionStyle());

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
        chLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(Style::kTextSecondary));
        chLabel->setFixedWidth(44);
        row->addWidget(chLabel);

        m_tagsLbl[i] = new QLabel(QStringLiteral("\u2014"), body);
        m_tagsLbl[i]->setStyleSheet(QStringLiteral("QLabel { color: #506070; font-size: 11px; }"));
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
        m_muteBtn[i]->setStyleSheet(vaxButtonStyle() + Style::greenCheckedStyle());
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
        m_deviceLbl[i]->setStyleSheet(QStringLiteral("QLabel { color: #506070; font-size: 10px; }"));
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
        txLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(Style::kTextSecondary));
        txLabel->setFixedWidth(44);
        row->addWidget(txLabel);

        m_txTagsLbl = new QLabel(QStringLiteral("\u2014"), body);
        m_txTagsLbl->setStyleSheet(QStringLiteral("QLabel { color: #506070; font-size: 11px; }"));
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

    buildStationSection(body, vbox);

    // ── 20 Hz level poll (Pattern A — isolated per-applet timer) ──────
    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(50);  // 50 ms = 20 Hz, "quiet meter" feel
    connect(m_levelTimer, &QTimer::timeout, this, &VaxApplet::pollLevels);
}

// iPhone app plan Task 25 (R-IOS-18): the Core computer's VAX channels, as
// this computer's rows above show its own, each control going through the
// Core's `vax` object.
void VaxApplet::buildStationSection(QWidget* body, QVBoxLayout* vbox)
{
    m_stationSection = new QWidget(body);
    auto* box = new QVBoxLayout(m_stationSection);
    box->setContentsMargins(0, 4, 0, 0);
    box->setSpacing(4);

    auto* line = new QFrame(m_stationSection);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet(QStringLiteral("QFrame { color: #203040; }"));
    box->addWidget(line);

    m_stationTitle = new QLabel(QStringLiteral("Core computer"), m_stationSection);
    m_stationTitle->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; font-weight: bold; }"
                       "QLabel:disabled { color: %2; }")
            .arg(Style::kTextPrimary, Style::kDisabledText));
    m_stationTitle->setToolTip(QStringLiteral("The VAX channels on the computer the Core runs on"));
    box->addWidget(m_stationTitle);

    for (int i = 0; i < kChannels; ++i) {
        const int channel = i + 1;
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);

        auto* chLabel = new QLabel(QStringLiteral("VAX %1:").arg(channel), m_stationSection);
        chLabel->setStyleSheet(stationLabelStyle(Style::kTextSecondary, 11));
        chLabel->setFixedWidth(44);
        row->addWidget(chLabel);

        m_stationTagsLbl[i] = new QLabel(QStringLiteral("\u2014"), m_stationSection);
        m_stationTagsLbl[i]->setStyleSheet(stationLabelStyle("#506070", 11));
        m_stationTagsLbl[i]->setFixedWidth(56);
        row->addWidget(m_stationTagsLbl[i]);

        m_stationRxMeter[i] = new MeterSlider(m_stationSection);
        connect(m_stationRxMeter[i], &MeterSlider::gainChanged, this, [this, channel](float g) {
            if (m_stationVax) {
                m_stationVax->setRxGain(channel, g);
            }
        });
        row->addWidget(m_stationRxMeter[i], 1);

        m_stationMuteBtn[i] = new QPushButton(QStringLiteral("Mute"), m_stationSection);
        m_stationMuteBtn[i]->setCheckable(true);
        m_stationMuteBtn[i]->setStyleSheet(vaxButtonStyle() + Style::greenCheckedStyle());
        m_stationMuteBtn[i]->setFixedSize(46, 20);
        m_stationMuteBtn[i]->setToolTip(
            QStringLiteral("Mute VAX channel %1 on the computer the Core runs on").arg(channel));
        connect(m_stationMuteBtn[i], &QPushButton::toggled, this, [this, channel](bool on) {
            if (m_stationVax) {
                m_stationVax->setMuted(channel, on);
            }
        });
        row->addWidget(m_stationMuteBtn[i]);
        box->addLayout(row);

        auto* devRow = new QHBoxLayout;
        devRow->setContentsMargins(48, 0, 0, 0);
        devRow->setSpacing(0);
        m_stationDeviceLbl[i] = new QLabel(QString(), m_stationSection);
        m_stationDeviceLbl[i]->setStyleSheet(stationLabelStyle("#506070", 10));
        devRow->addWidget(m_stationDeviceLbl[i]);
        devRow->addStretch();
        box->addLayout(devRow);
    }

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    auto* txLabel = new QLabel(QStringLiteral("TX:"), m_stationSection);
    txLabel->setStyleSheet(stationLabelStyle(Style::kTextSecondary, 11));
    txLabel->setFixedWidth(44);
    row->addWidget(txLabel);
    m_stationTxTagsLbl = new QLabel(QStringLiteral("\u2014"), m_stationSection);
    m_stationTxTagsLbl->setStyleSheet(stationLabelStyle("#506070", 11));
    m_stationTxTagsLbl->setFixedWidth(56);
    row->addWidget(m_stationTxTagsLbl);
    m_stationTxMeter = new MeterSlider(m_stationSection);
    connect(m_stationTxMeter, &MeterSlider::gainChanged, this, [this](float g) {
        if (m_stationVax) {
            m_stationVax->setTxGain(g);
        }
    });
    row->addWidget(m_stationTxMeter, 1);
    auto* spacer = new QWidget(m_stationSection);
    spacer->setFixedWidth(46);
    row->addWidget(spacer);
    box->addLayout(row);

    vbox->addWidget(m_stationSection);
    // A window that runs the radio directly can never have the section:
    // its own rows above are the Core computer's, so it is hidden there.
    // In a remote window it stays in place, disabled with its reason,
    // until the Core sends its VAX.
    m_stationSection->setVisible(m_model != nullptr && !m_model->ownsLocalDsp());
    applyStationAvailability();
    setStationTransmitPermitted(false, QString());
}

QString VaxApplet::stationUnavailableReason() const
{
    // Only a remote window shows the section (a local one hides it).
    return QStringLiteral("The Core computer is not sharing its VAX channels.");
}

void VaxApplet::applyStationAvailability()
{
    if (!m_stationSection) {
        return;
    }
    const QString reason = m_stationShown ? QString() : stationUnavailableReason();
    m_stationSection->setEnabled(m_stationShown);
    showStationReason(m_stationSection, m_stationShown, reason);
    showStationReason(m_stationTitle, m_stationShown, reason);
    for (int i = 0; i < kChannels; ++i) {
        showStationReason(m_stationRxMeter[i], m_stationShown, reason);
        showStationReason(m_stationMuteBtn[i], m_stationShown, reason);
    }
    updateStationTxRow();
}

void VaxApplet::setStationVax(StationVax* vax, bool shown)
{
    if (m_stationVax != vax) {
        if (m_stationVax) {
            disconnect(m_stationVax, nullptr, this, nullptr);
        }
        m_stationVax = vax;
        if (vax != nullptr) {
            connect(vax, &StationVax::slicesChanged, this, &VaxApplet::refreshStationValues);
            connect(vax, &StationVax::gainsChanged, this, &VaxApplet::refreshStationValues);
            connect(vax, &StationVax::mutesChanged, this, &VaxApplet::refreshStationValues);
            connect(vax, &StationVax::devicesChanged, this, &VaxApplet::refreshStationValues);
            connect(vax, &StationVax::levelsChanged, this, &VaxApplet::refreshStationLevels);
        }
    }
    m_stationShown = shown && vax != nullptr;
    applyStationAvailability();
    refreshStationValues();
    refreshStationLevels();
    updateStationLevelsWanted();
}

void VaxApplet::setStationTransmitPermitted(bool permitted, const QString& reason)
{
    m_stationTxPermitted = permitted;
    m_stationTxReason = reason;
    updateStationTxRow();
}

void VaxApplet::updateStationTxRow()
{
    if (!m_stationTxMeter) {
        return;
    }
    // The section's own reason comes first: while the section cannot be
    // used, the transmit permission is not what stops the row.
    m_stationTxMeter->setEnabled(m_stationTxPermitted);
    if (!m_stationShown) {
        m_stationTxMeter->setToolTip(stationUnavailableReason());
        return;
    }
    m_stationTxMeter->setToolTip(
        m_stationTxPermitted
            ? QStringLiteral("Level of VAX used as the microphone on the computer the "
                             "Core runs on")
            : (m_stationTxReason.isEmpty()
                   ? tr("Transmit controls are unavailable until the Core confirms "
                        "transmit permission.")
                   : m_stationTxReason));
}

void VaxApplet::refreshStationValues()
{
    StationVax* vax = m_stationVax.data();
    if (vax == nullptr) {
        return;
    }
    const auto tag = [](const QString& letters) {
        if (letters.isEmpty()) {
            return QStringLiteral("\u2014");
        }
        QStringList parts;
        for (const QChar letter : letters) {
            parts << QString(letter);
        }
        // As this computer's rows show theirs ("Slice A+B").
        return QStringLiteral("Slice %1").arg(parts.join(QLatin1Char('+')));
    };
    for (int i = 0; i < kChannels; ++i) {
        const int channel = i + 1;
        m_stationTagsLbl[i]->setText(tag(vax->property(
            QStringLiteral("ch%1Slices").arg(channel).toLatin1().constData()).toString()));
        m_stationDeviceLbl[i]->setText(vax->device(channel));
        {
            QSignalBlocker b(m_stationRxMeter[i]);
            m_stationRxMeter[i]->setGain(static_cast<float>(vax->rxGain(channel)));
        }
        {
            QSignalBlocker b(m_stationMuteBtn[i]);
            m_stationMuteBtn[i]->setChecked(vax->muted(channel));
        }
    }
    m_stationTxTagsLbl->setText(tag(vax->txSlice()));
    QSignalBlocker b(m_stationTxMeter);
    m_stationTxMeter->setGain(static_cast<float>(vax->txGain()));
}

void VaxApplet::refreshStationLevels()
{
    StationVax* vax = m_stationVax.data();
    if (vax == nullptr) {
        return;
    }
    for (int i = 0; i < kChannels; ++i) {
        m_stationRxMeter[i]->setLevel(static_cast<float>(vax->stationLevel(i + 1)));
    }
    m_stationTxMeter->setLevel(static_cast<float>(vax->stationTxLevel()));
}

void VaxApplet::updateStationLevelsWanted()
{
    const bool wanted = m_stationShown && isVisible();
    if (wanted == m_stationLevelsWanted) {
        return;
    }
    m_stationLevelsWanted = wanted;
    emit stationLevelsWantedChanged(wanted);
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
        SliceModel* s = m_model->sliceById(index);
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
    // The Core computer's meters only while they can be seen.
    updateStationLevelsWanted();
}

void VaxApplet::hideEvent(QHideEvent* e)
{
    if (m_levelTimer) {
        m_levelTimer->stop();
    }
    AppletWidget::hideEvent(e);
    updateStationLevelsWanted();
}

} // namespace NereusSDR
