// =================================================================
// src/gui/setup/AudioBackendStrip.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original diagnostic strip shown at the top of each
// Setup → Audio sub-page.
// See AudioBackendStrip.h for the full header.
//
// Task 17 (ubuntu-dev, 2026-04-24): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/setup/AudioBackendStrip.h"
#include "core/AudioEngine.h"

#include <QDesktopServices>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif

namespace NereusSDR {

AudioBackendStrip::AudioBackendStrip(AudioEngine* eng, QWidget* parent)
    : QWidget(parent)
    , m_eng(eng)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(8);

    // Backend name in bold
    m_primaryLabel = new QLabel(this);
    QFont boldFont = m_primaryLabel->font();
    boldFont.setBold(true);
    m_primaryLabel->setFont(boldFont);

    // Version / socket detail
    m_detailsLabel = new QLabel(this);

    m_rescan   = new QPushButton(QStringLiteral("Rescan"),    this);
    m_openLogs = new QPushButton(QStringLiteral("Open logs"), this);

    layout->addWidget(m_primaryLabel);
    layout->addWidget(m_detailsLabel);
    layout->addStretch();
    layout->addWidget(m_rescan);
    layout->addWidget(m_openLogs);

    connect(m_rescan,   &QPushButton::clicked, this, &AudioBackendStrip::onRescanClicked);
    connect(m_openLogs, &QPushButton::clicked, this, &AudioBackendStrip::onOpenLogsClicked);

#if defined(Q_OS_LINUX)
    if (m_eng) {
        connect(m_eng, &AudioEngine::linuxBackendChanged,
                this, [this](LinuxAudioBackend, LinuxAudioBackend) {
                    refresh();
                });
    }
#else
    if (m_rescan) {
        m_rescan->setEnabled(false);
    }
#endif

    // Initial paint.
    refresh();
}

void AudioBackendStrip::refresh()
{
#if defined(Q_OS_LINUX)
    if (m_eng) {
        const LinuxAudioBackend backend = m_eng->linuxBackend();
        const QString name = NereusSDR::toString(backend);
        m_primaryLabel->setText(name);

        switch (backend) {
            case LinuxAudioBackend::PipeWire:
                m_detailsLabel->setText(QStringLiteral("PipeWire detected"));
                break;
            case LinuxAudioBackend::Pactl:
                m_detailsLabel->setText(QStringLiteral("Pactl detected"));
                break;
            case LinuxAudioBackend::None:
                m_detailsLabel->setText(QStringLiteral("No backend detected"));
                break;
        }
    } else {
        m_primaryLabel->setText(QStringLiteral("None"));
        m_detailsLabel->setText(QStringLiteral("No AudioEngine"));
    }
#else
    m_primaryLabel->setText(QStringLiteral("None"));
    m_detailsLabel->setText(QStringLiteral("Linux audio backend not available on this platform"));
#endif
}

void AudioBackendStrip::onRescanClicked()
{
#if defined(Q_OS_LINUX)
    if (m_eng) {
        m_eng->rescanLinuxBackend();
        // Refresh is triggered by linuxBackendChanged signal if the
        // backend changes; call explicitly in case it stays the same.
        refresh();
    }
#endif
}

void AudioBackendStrip::onOpenLogsClicked()
{
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
}

} // namespace NereusSDR
