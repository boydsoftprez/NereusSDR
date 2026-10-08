// =================================================================
// src/gui/setup/SoundSystemLine.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original "Sound system" status line for Setup > Audio >
// Outputs. See SoundSystemLine.h for the full header.
//
// 2026-10-06: Written for the radio speaker and Audio Setup plan, Task 9,
// by J.J. Boyd (KG4VCF), with AI-assisted implementation via Anthropic
// Claude Code.
// =================================================================

#include "SoundSystemLine.h"

#include "core/AudioEngine.h"

#include <QHBoxLayout>
#include <QLabel>

namespace NereusSDR {

namespace {

constexpr int kDotPx = 8;

const char* const kDotOk =
    "QLabel { background: #33dd88; border-radius: 4px; }";
const char* const kDotProblem =
    "QLabel { background: #e04848; border-radius: 4px; }";
const char* const kTextOk = "QLabel { color: #8aa8c0; font-size: 12px; }";
const char* const kTextProblem = "QLabel { color: #e04848; font-size: 12px; }";

} // namespace

SoundSystemLine::SoundSystemLine(AudioEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_engine(engine)
{
    setObjectName(QStringLiteral("soundSystemLine"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_dot = new QLabel(this);
    m_dot->setObjectName(QStringLiteral("soundSystemDot"));
    m_dot->setFixedSize(kDotPx, kDotPx);
    layout->addWidget(m_dot, 0, Qt::AlignVCenter);

    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("soundSystemText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);
    layout->addWidget(m_text, 1);

#if defined(Q_OS_LINUX)
    if (m_engine != nullptr) {
        connect(m_engine, &AudioEngine::linuxBackendChanged, this,
                [this](LinuxAudioBackend, LinuxAudioBackend) { refresh(); });
    }
#endif
    refresh();
}

SoundSystemLine::System SoundSystemLine::thisSystem()
{
#if defined(Q_OS_MAC)
    return System::Mac;
#elif defined(Q_OS_WIN)
    return System::Windows;
#else
    return System::Linux;
#endif
}

QString SoundSystemLine::describe(System system, LinuxAudioBackend backend)
{
    switch (system) {
    case System::Mac:
        return tr("Core Audio");
    case System::Windows:
        // PortAudio makes its first Windows host API its default, and MME
        // is first in its list, so a device card left at "(PortAudio
        // default)" opens through MME.
        // From PortAudio src/os/win/pa_win_hostapis.c:72-100 [v19.7.0]
        return tr("Windows audio. Speakers and microphone use MME unless you pick "
                  "another driver, such as WASAPI, under Device details.");
    case System::Linux:
        break;
    }
    switch (backend) {
    case LinuxAudioBackend::PipeWire:
        return tr("PipeWire. NereusSDR talks to it directly.");
    case LinuxAudioBackend::Pactl:
        return tr("PulseAudio. PipeWire was not found, so NereusSDR uses the pactl "
                  "tool instead.");
    case LinuxAudioBackend::None:
        break;
    }
    return tr("None found. Start PipeWire or PulseAudio, then click Rescan devices.");
}

bool SoundSystemLine::isProblem(System system, LinuxAudioBackend backend)
{
    return system == System::Linux && backend == LinuxAudioBackend::None;
}

QString SoundSystemLine::text() const
{
    return m_described;
}

void SoundSystemLine::refresh()
{
    LinuxAudioBackend backend = LinuxAudioBackend::None;
#if defined(Q_OS_LINUX)
    if (m_engine != nullptr) {
        backend = m_engine->linuxBackend();
    }
#endif
    show(thisSystem(), backend);
}

void SoundSystemLine::show(System system, LinuxAudioBackend backend)
{
    m_described = describe(system, backend);
    m_problem = isProblem(system, backend);
    m_dot->setStyleSheet(QLatin1String(m_problem ? kDotProblem : kDotOk));
    m_text->setStyleSheet(QLatin1String(m_problem ? kTextProblem : kTextOk));
    m_text->setText(QStringLiteral("<b style=\"color:#c8d8e8\">%1</b> %2")
                        .arg(tr("Sound system:"), m_described.toHtmlEscaped()));
}

} // namespace NereusSDR
