// =================================================================
// src/core/audio/AudioStreamSupervisor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The stream supervisor (R-AUD-08 to
// R-AUD-14, D4 to D6, D27 to D30); no upstream logic.  The rules are in
// the header.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 5 (R-AUD-08 to R-AUD-14). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan early-review fix wave (R-AUD-08):
//               onRoleClosed().  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: early-review fix wave follow-up (R-AUD-08, R-AUD-14):
//               startMicOnly() and startOutputs().  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-11): a chosen output whose
//               fallback opens on the system default that is the chosen
//               device itself takes that open as the chosen device's, with
//               no retry.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/audio/AudioStreamSupervisor.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDeviceMatching.h"

#include <QtGlobal>

#include <algorithm>

namespace NereusSDR {

namespace {

constexpr AudioRole kOutputRoles[] = {AudioRole::Speakers, AudioRole::Headphones, AudioRole::Vax1,
                                      AudioRole::Vax2, AudioRole::Vax3, AudioRole::Vax4};

AudioRoleReason reasonFor(AudioOpenResult result)
{
    return result == AudioOpenResult::InUse ? AudioRoleReason::InUse : AudioRoleReason::NotConnected;
}

bool isBluetooth(const std::optional<AudioDeviceInfo>& device)
{
    return device && device->transport == AudioTransport::Bluetooth;
}

} // namespace

AudioStreamSupervisor::AudioStreamSupervisor(IAudioDeviceCatalog& catalogue, IAudioStreamHost& host,
                                             QObject* parent)
    : QObject(parent), m_catalogue(catalogue), m_host(host)
{
    for (int i = 0; i < kAudioRoleCount; ++i) {
        const AudioRole role = roleAt(i);
        RoleState& s = m_roles[static_cast<size_t>(i)];
        s.retryTimer.setSingleShot(true);
        connect(&s.retryTimer, &QTimer::timeout, this, [this, role]() { onRetryTimer(role); });
    }
    m_bluetoothWait.setSingleShot(true);
    connect(&m_bluetoothWait, &QTimer::timeout, this, [this]() { releaseHeldOutputs(); });

    connect(&m_catalogue, &IAudioDeviceCatalog::devicesChanged, this, [this]() {
        if (!m_started) {
            return;
        }
        for (int i = 0; i < kAudioRoleCount; ++i) {
            evaluate(roleAt(i), false);
        }
    });
    connect(&m_catalogue, &IAudioDeviceCatalog::defaultChanged, this,
            [this](AudioDeviceDirection direction) {
                if (!m_started) {
                    return;
                }
                if (direction == AudioDeviceDirection::Input) {
                    if (m_transmitting) {
                        // R-AUD-13: the mic never switches on the air; unkey applies it.
                        m_micDefaultMovedWhileTransmitting = true;
                        return;
                    }
                    evaluate(AudioRole::TxInput, true);
                    return;
                }
                for (AudioRole role : kOutputRoles) {
                    evaluate(role, true);
                }
            });

    // Every setChoice() made in this turn is known before anything opens.
    QMetaObject::invokeMethod(this, [this]() { start(); }, Qt::QueuedConnection);
}

AudioStreamSupervisor::~AudioStreamSupervisor() = default;

// ---------------------------------------------------------------------------
// Public calls
// ---------------------------------------------------------------------------

void AudioStreamSupervisor::setChoice(AudioRole role, const AudioDeviceConfig& config)
{
    RoleState& s = state(role);
    const bool had = s.hasChoice;
    const AudioDeviceConfig old = s.config;
    s.config = config;
    s.hasChoice = true;
    if (had && old == config) {
        return;
    }

    // Only the identity changed (a learned ID saved back, or another
    // device): the device decides whether to reopen.  Any other field is a
    // stream setting, so the open stream is reopened with it.
    AudioDeviceConfig sameIdentity = old;
    sameIdentity.deviceId = config.deviceId;
    sameIdentity.deviceName = config.deviceName;
    s.forceReopen = had && !(sameIdentity == config);

    s.learnedEmitted = false;
    s.trouble = AudioRoleReason::None;
    s.silentReason = AudioRoleReason::None;
    s.target.reset();
    cancelRetry(role);
    if (!m_started) {
        return;
    }
    if (s.pending) {
        closeIfOpen(role);   // the old open is no longer wanted
    }
    evaluate(role, false);
}

void AudioStreamSupervisor::setRoleEnabled(AudioRole role, bool enabled)
{
    RoleState& s = state(role);
    if (s.enabled == enabled) {
        return;
    }
    s.enabled = enabled;
    evaluate(role, false);
}

void AudioStreamSupervisor::setTransmitting(bool transmitting)
{
    if (m_transmitting == transmitting) {
        return;
    }
    m_transmitting = transmitting;
    if (transmitting) {
        return;
    }
    const bool moved = m_micDefaultMovedWhileTransmitting;
    m_micDefaultMovedWhileTransmitting = false;
    evaluate(AudioRole::TxInput, moved);
}

void AudioStreamSupervisor::setNoneMeansWaitingForPick(AudioRole role, bool waiting)
{
    RoleState& s = state(role);
    if (s.noneWaiting == waiting) {
        return;
    }
    s.noneWaiting = waiting;
    evaluate(role, false);
}

void AudioStreamSupervisor::onStreamEvent(AudioRole role, const AudioStreamEvent& event)
{
    RoleState& s = state(role);
    if (!m_started || !s.open) {
        return;   // nothing open: a late event from a stream already replaced
    }

    if (event.kind == AudioStreamEvent::Kind::FormatChanged
        || event.kind == AudioStreamEvent::Kind::ResetRequested) {
        // Reopen on the same device (spec design choice 3).
        s.forceReopen = true;
        if (s.onDefault) {
            if (!openSystemDefault(role)) {
                closeIfOpen(role);
                armRetry(role);
            }
        } else if (s.target) {
            attemptTarget(role);
        }
        publish(role);
        return;
    }

    const AudioRoleReason reason = event.kind == AudioStreamEvent::Kind::DeviceBusy
                                       ? AudioRoleReason::InUse
                                       : AudioRoleReason::NotConnected;

    if (s.onDefault) {
        // The system default's own stream went away: open whatever the
        // default is now, without waiting for the list.
        if (!openSystemDefault(role)) {
            closeIfOpen(role);
            if (kindOf(role) != ChoiceKind::Chosen) {
                s.silentReason = reason;
                if (systemDefault(role)) {
                    armRetry(role);
                }
            } else {
                s.silentReason = s.trouble;
            }
        }
        publish(role);
        return;
    }

    // The chosen (or resolved) device went away or was taken.
    markTrouble(role, reason);
    cancelRetry(role);
    if (fallsBackToDefault(role)) {
        // R-AUD-08, R-AUD-11: play on the system default at once.
        if (!systemDefault(role) || !openSystemDefault(role)) {
            closeIfOpen(role);
            s.silentReason = reason;
        }
    } else {
        // R-AUD-09, R-AUD-10: silent, never another device.
        closeIfOpen(role);
    }
    // An open the fallback took as the chosen device's needs no retry.
    const bool adopted = s.open && !s.onDefault;
    if (!adopted && s.target && listedById(role, s.target->id)) {
        armRetry(role);
    }
    publish(role);
}

void AudioStreamSupervisor::onOpenFinished(AudioRole role, AudioOpenResult result)
{
    RoleState& s = state(role);
    if (s.pending && result != AudioOpenResult::Pending) {
        const AudioDeviceInfo device = *s.pendingDevice;
        s.pending = false;
        s.pendingDevice.reset();
        applyTargetResult(role, result, device);
        publish(role);
    }
    if (role == AudioRole::TxInput && result != AudioOpenResult::Pending) {
        releaseHeldOutputs();
    }
}

void AudioStreamSupervisor::onRoleClosed(AudioRole role)
{
    // The failed open that follows reads it as closed: the fallback opens
    // the system default, and a role on the default retries.
    RoleState& s = state(role);
    s.open = false;
    s.onDefault = false;
    s.openDevice.reset();
}

AudioRoleStatus AudioStreamSupervisor::status(AudioRole role) const
{
    return state(role).status;
}

void AudioStreamSupervisor::setRetryScaleForTest(double scale)
{
    m_retryScale = scale;
}

int AudioStreamSupervisor::retryDelayMsForTest(AudioRole role) const
{
    return state(role).armedDelayMs;
}

void AudioStreamSupervisor::fireRetryForTest(AudioRole role)
{
    RoleState& s = state(role);
    if (s.armedDelayMs < 0) {
        return;
    }
    s.retryTimer.stop();
    onRetryTimer(role);
}

// ---------------------------------------------------------------------------
// Roles and the catalogue
// ---------------------------------------------------------------------------

bool AudioStreamSupervisor::isOutput(AudioRole role)
{
    return role != AudioRole::TxInput;
}

bool AudioStreamSupervisor::isVax(AudioRole role)
{
    return role == AudioRole::Vax1 || role == AudioRole::Vax2 || role == AudioRole::Vax3
           || role == AudioRole::Vax4;
}

bool AudioStreamSupervisor::fallsBackToDefault(AudioRole role)
{
    return role == AudioRole::Speakers || role == AudioRole::Headphones;
}

AudioDeviceDirection AudioStreamSupervisor::directionOf(AudioRole role)
{
    return isOutput(role) ? AudioDeviceDirection::Output : AudioDeviceDirection::Input;
}

int AudioStreamSupervisor::indexOf(AudioRole role)
{
    return static_cast<int>(role);
}

AudioRole AudioStreamSupervisor::roleAt(int index)
{
    return static_cast<AudioRole>(index);
}

AudioStreamSupervisor::RoleState& AudioStreamSupervisor::state(AudioRole role)
{
    return m_roles[static_cast<size_t>(indexOf(role))];
}

const AudioStreamSupervisor::RoleState& AudioStreamSupervisor::state(AudioRole role) const
{
    return m_roles[static_cast<size_t>(indexOf(role))];
}

AudioStreamSupervisor::ChoiceKind AudioStreamSupervisor::kindOf(AudioRole role) const
{
    const RoleState& s = state(role);
    if (!s.enabled || !s.hasChoice || s.config.isNone()) {
        return ChoiceKind::Off;
    }
    if (isVax(role) && s.config.isPlatformDefault()) {
        // An empty VAX device never opens the system default: with no
        // virtual cable that is the speakers, and raw VAX audio would
        // play through them ahead of the master volume.  It reads as
        // "(none)".
        return ChoiceKind::Off;
    }
    if (s.config.isPlatformDefault()) {
        return ChoiceKind::PlatformDefault;
    }
    return ChoiceKind::Chosen;
}

AudioEngineKind AudioStreamSupervisor::engineOf(AudioRole role) const
{
    // No Engine key: migration was postponed, so older drivers (Task 4).
    return state(role).config.engine.value_or(AudioEngineKind::PortAudio);
}

AudioBackendId AudioStreamSupervisor::backendOf(AudioRole role) const
{
    return audioBackendFor(engineOf(role));
}

QList<AudioDeviceInfo> AudioStreamSupervisor::listed(AudioRole role) const
{
    QList<AudioDeviceInfo> present;
    const QList<AudioDeviceInfo> all = m_catalogue.devices(backendOf(role), directionOf(role));
    for (const AudioDeviceInfo& device : all) {
        if (device.state != AudioDeviceState::NotConnected) {
            present.append(device);
        }
    }
    return present;
}

std::optional<AudioDeviceInfo> AudioStreamSupervisor::listedById(AudioRole role, const QString& id) const
{
    const QList<AudioDeviceInfo> present = listed(role);
    for (const AudioDeviceInfo& device : present) {
        if (device.id == id) {
            return device;
        }
    }
    return std::nullopt;
}

std::optional<AudioDeviceInfo> AudioStreamSupervisor::systemDefault(AudioRole role) const
{
    std::optional<AudioDeviceInfo> def = m_catalogue.defaultDevice(backendOf(role), directionOf(role));
    if (def && def->state == AudioDeviceState::NotConnected) {
        return std::nullopt;
    }
    return def;
}

std::optional<AudioDeviceInfo> AudioStreamSupervisor::resolvePlatformMic() const
{
    // D6, R-AUD-13: never a Bluetooth mic, not even as the system default.
    const std::optional<AudioDeviceInfo> def = systemDefault(AudioRole::TxInput);
    if (def && !isBluetooth(def)) {
        return def;
    }
    const QList<AudioDeviceInfo> inputs = listed(AudioRole::TxInput);
    for (const AudioDeviceInfo& device : inputs) {
        if (device.transport == AudioTransport::BuiltIn) {
            return device;
        }
    }
    for (const AudioDeviceInfo& device : inputs) {
        if (device.transport != AudioTransport::Bluetooth) {
            return device;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

void AudioStreamSupervisor::start()
{
    if (m_started) {
        return;
    }
    startMicOnly();
    startOutputs();
}

void AudioStreamSupervisor::startMicOnly()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_outputsDeferred = true;
    evaluate(AudioRole::TxInput, false);
}

void AudioStreamSupervisor::startOutputs()
{
    if (!m_started) {
        start();
        return;
    }
    if (!m_outputsDeferred) {
        return;
    }
    m_outputsDeferred = false;

    // R-AUD-14: the mic opens first, so a Bluetooth headset that is both
    // mic and speakers settles on its mic before any output opens.
    const RoleState& mic = state(AudioRole::TxInput);
    if (mic.pending && isBluetooth(mic.pendingDevice)) {
        m_outputsHeld = true;
        m_bluetoothWait.start(scaled(kBluetoothMicFirstWaitMs));
    }
    for (AudioRole role : kOutputRoles) {
        evaluate(role, false);
    }
}

void AudioStreamSupervisor::evaluate(AudioRole role, bool defaultMoved)
{
    if (!m_started) {
        return;
    }
    const ChoiceKind kind = kindOf(role);
    if (kind == ChoiceKind::Off) {
        evaluateOff(role);
    } else if ((m_outputsHeld || m_outputsDeferred) && isOutput(role)) {
        // Waiting for the Bluetooth mic to open first, or for startOutputs().
    } else if (kind == ChoiceKind::Chosen) {
        evaluateChosen(role, defaultMoved);
    } else if (role == AudioRole::TxInput) {
        evaluateDefaultMic(role, defaultMoved);
    } else {
        evaluateDefaultOutput(role, defaultMoved);
    }
    publish(role);
}

void AudioStreamSupervisor::evaluateOff(AudioRole role)
{
    RoleState& s = state(role);
    cancelRetry(role);
    closeIfOpen(role);
    s.target.reset();
    s.trouble = AudioRoleReason::None;
    s.silentReason = AudioRoleReason::None;
    s.forceReopen = false;
}

void AudioStreamSupervisor::evaluateChosen(AudioRole role, bool defaultMoved)
{
    RoleState& s = state(role);
    const std::optional<AudioDeviceMatch> match = matchSavedAudioDevice(s.config, listed(role));
    if (match && !match->byId && !s.learnedEmitted) {
        s.learnedEmitted = true;
        emit savedIdentityLearned(role, match->device.id);
    }
    if (s.pending) {
        return;   // the mic's open is still in flight
    }
    if (s.open && !s.onDefault && s.openDevice && !s.forceReopen) {
        // Playing the chosen device: list changes never reopen it.  It is
        // left only on its own stream event (R-AUD-08).
        if (!match || match->device.id == s.openDevice->id) {
            return;
        }
    }

    if (!match) {
        // Not listed: not connected.  No retries until it is listed again.
        markTrouble(role, AudioRoleReason::NotConnected);
        cancelRetry(role);
        ensureFallback(role, defaultMoved);
        return;
    }

    s.target = match->device;
    if (s.trouble == AudioRoleReason::None) {
        attemptTarget(role);
        return;
    }
    // Listed again after trouble: open on the schedule (a Bluetooth device
    // can be listed before it can be opened).  Until then the outputs play
    // on the system default and the mic and VAX stay silent.
    if (s.armedDelayMs < 0) {
        armRetry(role);
    }
    ensureFallback(role, defaultMoved);
}

void AudioStreamSupervisor::evaluateDefaultOutput(AudioRole role, bool defaultMoved)
{
    RoleState& s = state(role);
    if (!systemDefault(role)) {
        cancelRetry(role);
        closeIfOpen(role);
        s.silentReason = AudioRoleReason::NoDevice;
        return;
    }
    if (!defaultMoved && !s.forceReopen) {
        if (s.open && s.onDefault) {
            return;
        }
        if (s.armedDelayMs >= 0) {
            return;   // a failed open waits for its retry
        }
    }
    // R-AUD-12: follow the system default at once.
    cancelRetry(role);
    if (!openSystemDefault(role)) {
        if (!(s.open && s.onDefault)) {
            closeIfOpen(role);
        }
        armRetry(role);
    }
}

void AudioStreamSupervisor::evaluateDefaultMic(AudioRole role, bool defaultMoved)
{
    RoleState& s = state(role);
    if (s.pending) {
        return;
    }

    if (m_transmitting) {
        // R-AUD-13: never switch the mic while transmitting.  The same
        // device may still come back on its retries.
        if (s.target && listedById(role, s.target->id)) {
            if (s.trouble != AudioRoleReason::None && s.armedDelayMs < 0) {
                armRetry(role);
            }
        } else if (s.target || s.open) {
            cancelRetry(role);
            markTrouble(role, AudioRoleReason::NotConnected);
            closeIfOpen(role);
            m_micDefaultMovedWhileTransmitting = true;
        } else {
            m_micDefaultMovedWhileTransmitting = true;
        }
        return;
    }

    const std::optional<AudioDeviceInfo> def = systemDefault(role);
    const std::optional<AudioDeviceInfo> current = s.target ? listedById(role, s.target->id) : std::nullopt;
    std::optional<AudioDeviceInfo> next;
    if (defaultMoved && def && !isBluetooth(def)) {
        next = def;           // follow the default input
    } else if (current) {
        next = current;       // stay on the mic it has (also when the default became Bluetooth)
    } else {
        next = resolvePlatformMic();
    }

    if (!next) {
        cancelRetry(role);
        closeIfOpen(role);
        s.target.reset();
        s.trouble = AudioRoleReason::None;
        s.silentReason = AudioRoleReason::NoDevice;
        return;
    }
    if (!s.target || s.target->id != next->id) {
        s.trouble = AudioRoleReason::None;
        s.silentReason = AudioRoleReason::None;
        cancelRetry(role);
    }
    s.target = next;
    if (s.open && s.openDevice && s.openDevice->id == next->id && !s.forceReopen) {
        return;
    }
    if (s.trouble != AudioRoleReason::None) {
        if (s.armedDelayMs < 0) {
            armRetry(role);
        }
        return;
    }
    attemptTarget(role);
}

// ---------------------------------------------------------------------------
// Opening and closing
// ---------------------------------------------------------------------------

void AudioStreamSupervisor::attemptTarget(AudioRole role)
{
    RoleState& s = state(role);
    if (!s.target) {
        return;
    }
    s.forceReopen = false;
    const AudioDeviceInfo device = *s.target;
    const AudioOpenResult result = m_host.openRole(role, engineOf(role), device, s.config);
    applyTargetResult(role, result, device);
}

void AudioStreamSupervisor::applyTargetResult(AudioRole role, AudioOpenResult result,
                                              const AudioDeviceInfo& device)
{
    RoleState& s = state(role);
    switch (result) {
    case AudioOpenResult::Opened:
        s.open = true;
        s.onDefault = false;
        s.openDevice = device;
        s.trouble = AudioRoleReason::None;
        s.silentReason = AudioRoleReason::None;
        cancelRetry(role);
        return;
    case AudioOpenResult::Pending:
        // The capture helper opens it; onOpenFinished() completes it.  The
        // role has nothing it can record from until then.
        s.open = false;
        s.onDefault = false;
        s.openDevice.reset();
        s.pending = true;
        s.pendingDevice = device;
        s.silentReason = s.trouble;
        return;
    case AudioOpenResult::InUse:
    case AudioOpenResult::NotFound:
    case AudioOpenResult::Failed:
        break;
    }

    if (s.trouble == AudioRoleReason::None) {
        // Once per run of failures; the retries that follow are not logged.
        qCWarning(lcAudio) << "Audio role" << indexOf(role) << "could not open" << device.name;
    }
    markTrouble(role, reasonFor(result));
    armRetry(role);
    if (fallsBackToDefault(role)) {
        ensureFallback(role, false);
        return;
    }
    // The mic on "(platform default)" keeps the mic it has (the host left
    // it open); a chosen mic or a VAX channel never keeps another device.
    const bool keepsOther = kindOf(role) == ChoiceKind::PlatformDefault && s.open && s.openDevice
                            && s.openDevice->id != device.id;
    if (!keepsOther) {
        closeIfOpen(role);
    }
}

bool AudioStreamSupervisor::openSystemDefault(AudioRole role)
{
    RoleState& s = state(role);
    s.forceReopen = false;
    const AudioOpenResult result = m_host.openRole(role, engineOf(role), std::nullopt, s.config);
    if (result == AudioOpenResult::Opened) {
        s.open = true;
        s.onDefault = true;
        s.openDevice.reset();
        if (kindOf(role) != ChoiceKind::Chosen) {
            s.silentReason = AudioRoleReason::None;
        }
        // Carried finding (Task 9, settled for JJ's veto): the system
        // default is the chosen device itself.  The fallback opened it with
        // the same engine and config, so it is the chosen device playing,
        // exclusive where chosen; a retry of the chosen device would only
        // collide with this open (in use) or reopen it with a gap.
        const std::optional<AudioDeviceInfo> def = systemDefault(role);
        if (kindOf(role) == ChoiceKind::Chosen && s.target && def && def->id == s.target->id) {
            s.onDefault = false;
            s.openDevice = *s.target;
            s.trouble = AudioRoleReason::None;
            s.silentReason = AudioRoleReason::None;
            cancelRetry(role);
        }
        return true;
    }
    if (s.armedDelayMs < 0 && s.retryStep == 0) {
        qCWarning(lcAudio) << "Audio role" << indexOf(role) << "could not open the system default";
    }
    if (kindOf(role) != ChoiceKind::Chosen) {
        s.silentReason = reasonFor(result);
    }
    return false;
}

void AudioStreamSupervisor::ensureFallback(AudioRole role, bool defaultMoved)
{
    RoleState& s = state(role);
    if (!fallsBackToDefault(role)) {
        closeIfOpen(role);
        s.silentReason = s.trouble;
        return;
    }
    if (!systemDefault(role)) {
        closeIfOpen(role);
        s.silentReason = s.trouble;
        return;
    }
    if (s.open && s.onDefault && !defaultMoved) {
        return;
    }
    if (!openSystemDefault(role)) {
        if (!(s.open && s.onDefault)) {
            closeIfOpen(role);
        }
        s.silentReason = s.trouble;
    }
}

void AudioStreamSupervisor::closeIfOpen(AudioRole role)
{
    RoleState& s = state(role);
    if (!s.open && !s.pending) {
        return;
    }
    m_host.closeRole(role);
    s.open = false;
    s.onDefault = false;
    s.openDevice.reset();
    s.pending = false;
    s.pendingDevice.reset();
}

void AudioStreamSupervisor::markTrouble(AudioRole role, AudioRoleReason reason)
{
    RoleState& s = state(role);
    s.trouble = reason;
    s.silentReason = reason;
}

int AudioStreamSupervisor::scaled(int ms) const
{
    return std::max(0, qRound(ms * m_retryScale));
}

void AudioStreamSupervisor::armRetry(AudioRole role)
{
    RoleState& s = state(role);
    const int last = static_cast<int>(kRetryMs.size()) - 1;
    const int delay = kRetryMs[static_cast<size_t>(std::min(s.retryStep, last))];
    s.retryStep = std::min(s.retryStep + 1, last);
    s.armedDelayMs = delay;
    s.retryTimer.start(scaled(delay));
}

void AudioStreamSupervisor::cancelRetry(AudioRole role)
{
    RoleState& s = state(role);
    s.retryTimer.stop();
    s.armedDelayMs = -1;
    s.retryStep = 0;
}

void AudioStreamSupervisor::onRetryTimer(AudioRole role)
{
    RoleState& s = state(role);
    s.armedDelayMs = -1;
    if (!m_started) {
        return;
    }
    const ChoiceKind kind = kindOf(role);
    if (kind == ChoiceKind::Off || s.pending
        || ((m_outputsHeld || m_outputsDeferred) && isOutput(role))) {
        return;
    }

    if (kind == ChoiceKind::Chosen) {
        const std::optional<AudioDeviceMatch> match = matchSavedAudioDevice(s.config, listed(role));
        if (!match) {
            evaluate(role, false);   // left the list: the retries stop
            return;
        }
        s.target = match->device;
        attemptTarget(role);
    } else if (role == AudioRole::TxInput) {
        const std::optional<AudioDeviceInfo> current =
            s.target ? listedById(role, s.target->id) : std::nullopt;
        if (!current) {
            evaluate(role, false);
            return;
        }
        s.target = current;
        attemptTarget(role);
    } else {
        if (!systemDefault(role)) {
            evaluate(role, false);
            return;
        }
        if (!openSystemDefault(role)) {
            armRetry(role);
        }
    }
    publish(role);
}

void AudioStreamSupervisor::releaseHeldOutputs()
{
    if (!m_outputsHeld) {
        return;
    }
    m_outputsHeld = false;
    m_bluetoothWait.stop();
    if (m_outputsDeferred) {
        return;   // startOutputs() evaluates them
    }
    for (AudioRole role : kOutputRoles) {
        evaluate(role, false);
    }
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

void AudioStreamSupervisor::publish(AudioRole role)
{
    RoleState& s = state(role);
    const ChoiceKind kind = kindOf(role);

    AudioRoleStatus status;
    status.chosen = s.config;
    if (kind == ChoiceKind::Chosen) {
        status.chosenName = s.config.deviceName;
        if (status.chosenName.isEmpty() && s.target) {
            status.chosenName = s.target->name;
        }
    }

    if (kind == ChoiceKind::Off) {
        const bool noDevice = s.config.isNone() || (isVax(role) && s.config.isPlatformDefault());
        const bool waiting = s.enabled && s.hasChoice && noDevice && s.noneWaiting;
        status.state = waiting ? AudioRoleState::WaitingForPick : AudioRoleState::Off;
    } else if (s.open && !s.onDefault && s.openDevice) {
        status.state = AudioRoleState::Playing;
        status.playingName = s.openDevice->name;
        status.playingBluetooth = isBluetooth(s.openDevice);
    } else if (s.open && s.onDefault) {
        const std::optional<AudioDeviceInfo> def = systemDefault(role);
        status.playingName = def ? def->name : QString();
        status.playingBluetooth = isBluetooth(def);
        if (kind == ChoiceKind::Chosen) {
            status.state = AudioRoleState::PlayingOnDefault;
            status.reason = s.trouble;
        } else {
            status.state = AudioRoleState::Playing;
        }
    } else {
        status.state = AudioRoleState::Silent;
        status.reason = s.silentReason;
    }

    if (status == s.status) {
        return;
    }
    s.status = status;
    emit statusChanged(role, status);
}

} // namespace NereusSDR
