// =================================================================
// src/core/audio/AudioBackendRegistry.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AudioBackendRegistry.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-01, R-AUD-02). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02): the Mac
//               registers Core Audio alone. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 9: Windows audio registers first on
//               Windows, the older drivers then without the host APIs it
//               replaces (R-AUD-01, R-AUD-02). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 10 (R-AUD-01): one branch per
//               system; Linux registers PipeWire ahead of the older
//               drivers, outside the Core. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-02, R-AUD-31):
//               Linux desktops register PipeWire, then PulseAudio, then the
//               older drivers without the host APIs they replace; both
//               native backends report the Linux engine selection.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioBackendRegistry.h"

#include "core/audio/PortAudioBackend.h"

#include <QtGlobal>

#if defined(Q_OS_MAC)
#include "core/audio/CoreAudioBackend.h"
#include "core/audio/CoreAudioSystem.h"
#elif defined(Q_OS_WIN)
#include "core/audio/WasapiBackendWin.h"
#elif defined(Q_OS_LINUX)
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/audio/LinuxEngineSelection.h"
#if defined(NEREUS_HAVE_PIPEWIRE)
#include "core/audio/PipeWireDeviceBackend.h"
#include "core/audio/PipeWireDeviceSystem.h"
#endif
#if defined(NEREUS_HAVE_PULSEAUDIO)
#include "core/audio/PulseAudioBackend.h"
#include "core/audio/PulseAudioSystem.h"
#endif
#endif

#include <optional>

namespace NereusSDR {

namespace {

// R-AUD-02: the engine a native backend stands for as the default.  ASIO
// is never a default, and PortAudio is the fallback, not a native choice.
std::optional<AudioEngineKind> nativeDefaultFor(AudioBackendId id)
{
    switch (id) {
    case AudioBackendId::CoreAudio:
        return AudioEngineKind::CoreAudio;
    case AudioBackendId::Wasapi:
        return AudioEngineKind::WindowsShared;
    case AudioBackendId::PipeWire:
        return AudioEngineKind::PipeWire;
    case AudioBackendId::PulseAudio:
        return AudioEngineKind::PulseAudio;
    case AudioBackendId::AlsaDirect:
        return AudioEngineKind::AlsaDirect;
    case AudioBackendId::PortAudio:
    case AudioBackendId::Asio:
        return std::nullopt;
    }
    return std::nullopt;
}

#if defined(Q_OS_LINUX)
// Audio/LinuxBackendPreferred, as detectLinuxBackend reads it; "pipewire"
// in a build without PipeWire is ignored, as it is there.
QString linuxForcedEngine()
{
    QString forced = AppSettings::instance()
                         .value(QStringLiteral("Audio/LinuxBackendPreferred"), QString())
                         .toString();
#if !defined(NEREUS_HAVE_PIPEWIRE)
    if (forced == QStringLiteral("pipewire")) {
        qCWarning(lcAudio) << "Audio/LinuxBackendPreferred is pipewire but this build has no PipeWire";
        forced.clear();
    }
#endif
    return forced;
}
#endif

} // namespace

std::vector<std::shared_ptr<IAudioEngineBackend>> makeSystemAudioBackends(const AudioBackendContext& context)
{
    // The native engines join this list in their own tasks, ahead of the
    // older drivers, and read the context then (the Core and the mic
    // helper register different engines).
    static_cast<void>(context);

    std::vector<std::shared_ptr<IAudioEngineBackend>> backends;
#if defined(Q_OS_MAC)
    // R-AUD-01: Core Audio is the Mac's only engine, in the window, the
    // Core and the mic helper alike; the older drivers add nothing here,
    // so PortAudio is not registered.
    backends.push_back(std::make_shared<CoreAudioBackend>(makeCoreAudioSystem()));
#elif defined(Q_OS_WIN)
    // R-AUD-02: Windows audio (shared and exclusive) comes first; the older
    // drivers then list only what it does not replace: MME, DirectSound
    // and WDM-KS (olderDriverHostApis in PortAudioBackend.cpp).
    backends.push_back(std::make_shared<WasapiBackendWin>());
    constexpr bool kIncludeReplacedHostApis = false;
    backends.push_back(std::make_shared<PortAudioBackend>(&listPortAudioDevices,
                                                          currentOlderDriverPlatform(),
                                                          kIncludeReplacedHostApis));
#else
    // R-AUD-01, Linux: PipeWire, then PulseAudio, then the older drivers.
    // The Core has no desktop session, so it registers neither (ALSA
    // direct, Task 12); the window and the mic helper register both.
    bool includeReplacedHostApis = true;
#if defined(Q_OS_LINUX)
    if (!context.daemon) {
        // R-AUD-31: one build carries both engines; which one runs is the
        // sound server that answers now (LinuxEngineSelection.h).  Both are
        // always registered, so the one that is not running is shown greyed
        // with its reason; the selection is read live, so PipeWire coming
        // back is seen without a restart.
#if defined(NEREUS_HAVE_PIPEWIRE)
        std::shared_ptr<IPipeWireDeviceSystem> pipeWire = makePipeWireDeviceSystem();
#endif
#if defined(NEREUS_HAVE_PULSEAUDIO)
        std::shared_ptr<IPulseAudioSystem> pulse = makePulseAudioSystem();
#endif
        auto selection = [
#if defined(NEREUS_HAVE_PIPEWIRE)
                             pipeWire,
#endif
#if defined(NEREUS_HAVE_PULSEAUDIO)
                             pulse,
#endif
                             forced = linuxForcedEngine()]() {
            LinuxSoundServerProbe probe;
            probe.forced = forced;
#if defined(NEREUS_HAVE_PIPEWIRE)
            probe.pipewireAnswers = pipeWire->running();
#endif
#if defined(NEREUS_HAVE_PULSEAUDIO)
            probe.pulseServerName = pulse->serverName();
#endif
            return chooseLinuxEngines(probe);
        };
#if defined(NEREUS_HAVE_PIPEWIRE)
        backends.push_back(std::make_shared<PipeWireDeviceBackend>(
            pipeWire, [selection] { return selection().pipewireRunning; }));
#endif
#if defined(NEREUS_HAVE_PULSEAUDIO)
        backends.push_back(std::make_shared<PulseAudioBackend>(
            pulse, [selection] { return selection().pulseRunning; }));
#endif
        static_cast<void>(selection);
        // R-AUD-01: the older drivers follow without the host APIs the
        // native engines replace (on Linux, olderDriverHostApis lists JACK
        // and ALSA either way).
        includeReplacedHostApis = false;
    }
#endif
    backends.push_back(std::make_shared<PortAudioBackend>(&listPortAudioDevices,
                                                          currentOlderDriverPlatform(),
                                                          includeReplacedHostApis));
#endif
    return backends;
}

AudioEngineKind defaultAudioEngine(const std::vector<std::shared_ptr<IAudioEngineBackend>>& backends)
{
    for (const std::shared_ptr<IAudioEngineBackend>& backend : backends) {
        if (!backend) {
            continue;
        }
        const std::optional<AudioEngineKind> kind = nativeDefaultFor(backend->id());
        if (kind && backend->running()) {
            return *kind;
        }
    }
    return AudioEngineKind::PortAudio;
}

} // namespace NereusSDR
