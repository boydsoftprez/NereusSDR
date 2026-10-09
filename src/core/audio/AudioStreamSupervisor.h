// =================================================================
// src/core/audio/AudioStreamSupervisor.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The stream supervisor: which device
// each audio role opens, what it reports, retries, and the Bluetooth mic
// rules (R-AUD-08 to R-AUD-14, D4 to D6, D27 to D30); no upstream logic.
//
// Main thread only.  The supervisor reads the live catalogue and asks the
// host to open and close each role's stream.  The rules:
//
// - Speakers and headphones whose chosen device goes away, or is held by
//   another program, play on the engine's system default at once (on the
//   stream event, before any list change) and move back when the device
//   opens again (R-AUD-08, R-AUD-11).
// - The PC mic (TxInput) and VAX 1 to 4 never move to another device: a
//   chosen device that goes away closes the role, which stays silent until
//   that device opens again (R-AUD-09, R-AUD-10).
// - A chosen device that is listed but fails to open is retried at
//   kRetryMs (250, 500, 1000, 2000 ms), then every 2000 ms while it stays
//   listed; leaving the list stops the retries and coming back starts the
//   schedule again from 250 ms.
// - Outputs on "(platform default)" follow the system default output at
//   once (R-AUD-12).  The mic on "(platform default)" follows the default
//   input, never while transmitting (the change applies at unkey) and
//   never to a Bluetooth mic (R-AUD-13); a Bluetooth default at start
//   opens the built-in input, else another input that is not Bluetooth,
//   else nothing.  The mic is always opened on an explicit device, never
//   on "whatever the engine's default is", so the engine cannot pick a
//   Bluetooth mic by itself (D6).
// - VAX 1 to 4 on an empty device never open anything (never the system
//   default, which without a virtual cable is the speakers); they read
//   as "(none)": Off, or WaitingForPick.
// - A Bluetooth mic picked by name opens.  When it is the mic's choice at
//   start, outputs wait for its open to finish, at most
//   kBluetoothMicFirstWaitMs, so the headset's mic opens first (R-AUD-14).
//
// Start: the constructor posts the first evaluation to the event loop, so
// every setChoice() made in the same turn is known before anything opens
// and the mic is opened before the outputs.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 5 (R-AUD-08 to R-AUD-14). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/AudioDeviceConfig.h"
#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/IAudioStreamHost.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <array>
#include <optional>

namespace NereusSDR {

class AudioStreamSupervisor final : public QObject {
    Q_OBJECT
public:
    static constexpr std::array<int, 4> kRetryMs{250, 500, 1000, 2000};  // then every 2000 ms while present
    static constexpr int kBluetoothMicFirstWaitMs = 2000;

    AudioStreamSupervisor(IAudioDeviceCatalog& catalogue, IAudioStreamHost& host, QObject* parent = nullptr);
    ~AudioStreamSupervisor() override;

    void setChoice(AudioRole role, const AudioDeviceConfig& config);
    void setRoleEnabled(AudioRole role, bool enabled);   // a disabled role is Off
    void setTransmitting(bool transmitting);
    void setNoneMeansWaitingForPick(AudioRole role, bool waiting);   // a "(none)" choice reads WaitingForPick, not Off
    void onStreamEvent(AudioRole role, const AudioStreamEvent& event);   // main thread
    void onOpenFinished(AudioRole role, AudioOpenResult result);          // completes a Pending open (the mic helper)
    AudioRoleStatus status(AudioRole role) const;

    // Scales every retry and the Bluetooth mic wait (0.01: a hundred times
    // faster).  Applies to timers armed after the call.
    void setRetryScaleForTest(double scale);
    // The armed retry's unscaled delay (250, 500, ...), -1 when none is armed.
    int retryDelayMsForTest(AudioRole role) const;
    // Runs the armed retry now, as its timer would.
    void fireRetryForTest(AudioRole role);

signals:
    void statusChanged(NereusSDR::AudioRole role, const NereusSDR::AudioRoleStatus& status);
    void savedIdentityLearned(NereusSDR::AudioRole role, const QString& deviceId);  // name match: save the ID back

private:
    enum class ChoiceKind { Off, PlatformDefault, Chosen };

    struct RoleState {
        AudioDeviceConfig config;
        bool hasChoice = false;
        bool enabled = true;
        bool noneWaiting = false;
        bool forceReopen = false;   // the stream settings changed, or the stream asked to be reset

        // What the role has open.  onDefault: an output opened on the
        // engine's system default (device nullopt).  Otherwise openDevice
        // is the explicit device.
        bool open = false;
        bool onDefault = false;
        std::optional<AudioDeviceInfo> openDevice;

        // A Pending open (the mic): the device it was asked for.
        bool pending = false;
        std::optional<AudioDeviceInfo> pendingDevice;

        // The target: the chosen device, or for the mic on "(platform
        // default)" the device it resolved to.  trouble is why the target
        // is not open (NotConnected or InUse), None when it is fine.
        std::optional<AudioDeviceInfo> target;
        AudioRoleReason trouble = AudioRoleReason::None;
        AudioRoleReason silentReason = AudioRoleReason::None;

        int retryStep = 0;
        int armedDelayMs = -1;
        QTimer retryTimer;

        bool learnedEmitted = false;
        AudioRoleStatus status;
    };

    static bool isOutput(AudioRole role);
    static bool isVax(AudioRole role);
    static bool fallsBackToDefault(AudioRole role);
    static AudioDeviceDirection directionOf(AudioRole role);
    static int indexOf(AudioRole role);
    static AudioRole roleAt(int index);

    RoleState& state(AudioRole role);
    const RoleState& state(AudioRole role) const;
    ChoiceKind kindOf(AudioRole role) const;
    AudioEngineKind engineOf(AudioRole role) const;
    AudioBackendId backendOf(AudioRole role) const;
    QList<AudioDeviceInfo> listed(AudioRole role) const;
    std::optional<AudioDeviceInfo> listedById(AudioRole role, const QString& id) const;
    std::optional<AudioDeviceInfo> systemDefault(AudioRole role) const;
    std::optional<AudioDeviceInfo> resolvePlatformMic() const;

    void start();
    void evaluate(AudioRole role, bool defaultMoved);
    void evaluateOff(AudioRole role);
    void evaluateChosen(AudioRole role, bool defaultMoved);
    void evaluateDefaultOutput(AudioRole role, bool defaultMoved);
    void evaluateDefaultMic(AudioRole role, bool defaultMoved);

    void attemptTarget(AudioRole role);
    void applyTargetResult(AudioRole role, AudioOpenResult result, const AudioDeviceInfo& device);
    bool openSystemDefault(AudioRole role);
    void ensureFallback(AudioRole role, bool defaultMoved);
    void closeIfOpen(AudioRole role);
    void markTrouble(AudioRole role, AudioRoleReason reason);
    int scaled(int ms) const;
    void armRetry(AudioRole role);
    void cancelRetry(AudioRole role);
    void onRetryTimer(AudioRole role);

    void releaseHeldOutputs();
    void publish(AudioRole role);

    IAudioDeviceCatalog& m_catalogue;
    IAudioStreamHost& m_host;
    std::array<RoleState, kAudioRoleCount> m_roles;
    bool m_started = false;
    bool m_transmitting = false;
    bool m_micDefaultMovedWhileTransmitting = false;
    bool m_outputsHeld = false;
    QTimer m_bluetoothWait;
    double m_retryScale = 1.0;
};

} // namespace NereusSDR
