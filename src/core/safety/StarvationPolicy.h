// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/StarvationPolicy.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 37 (R-IOS-13; remote design section 12.3, "uplink
// starvation while keyed"; spec section 4.6 item 2): what the Core does
// when a keyed device's microphone line stops carrying audio on a live
// link.
//
// The line counts as starved after 250 ms without audio while the device
// is keyed on it (RemoteMicReceiver::starved, RemoteMicConfig::
// kStarvationMs), which is shorter than the 400 ms link hold deadline
// (RemoteTxWatchdog::kLinkHoldMs). What happens then depends on the
// transmit mode:
//
//   KeepKeyed: LSB, USB, DSB, CWL, CWU, DIGL, DIGU, SPEC. Silence there
//     puts no carrier on the air, so the transmission goes on (silent).
//     DSB is here because WDSP's DSB modulator adds no carrier
//     (third_party/wdsp/src/ammod.c, xammod mode 1, out = mult * in).
//   Hold: AM, SAM, FM, DRM, RADE_U, RADE_L (and RADE2). Silence there is
//     a bare or garbage carrier, so the Core holds RF silent (holdRf(true))
//     while the key stays; audio arriving again releases it
//     (holdRf(false)).
//
// Remote transmit hold then cut (2026-10-26): in every mode, a keyed line
// with no audio packets for 10 s (RemoteMicReceiver::noAudio,
// RemoteMicConfig::kNoAudioCutMs) stops transmitting with "No microphone
// audio arrived from <device>, so the Core stopped transmitting." and
// releases a hold that is on.
//
// TUNE and two-tone use no microphone and are never held or stopped by it.
// A mode this table does not know is treated as Hold.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 37 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: Hold replaces Unkey (the
//               same modes), the holdRf hook and isHolding; onStarved
//               never stops, and onNoAudio is the 10 s cut in every mode.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

#include "core/WdspTypes.h"

namespace NereusSDR {

enum class StarvationAction {
    /// Silence puts no carrier on the air: stay keyed.
    KeepKeyed,
    /// Silence is a bare or garbage carrier: hold RF silent while keyed.
    Hold,
};

class StarvationPolicy {
public:
    struct Hooks {
        /// The transmit mode now (the transmit slice's), or nullopt when
        /// there is none.
        std::function<std::optional<DSPMode>()> transmitMode;
        /// True while TUNE or two-tone is on (they use no microphone).
        std::function<bool()> microphoneUnused;
        /// StopAllTx with `message`.
        std::function<void(const QString& message)> stopAllTx;
        /// The device's name as the Core shows it.
        std::function<QString(const QByteArray& deviceId)> deviceName;
        /// The Mic hold: true holds RF silent while keyed, false releases
        /// it. Unset: a no-op.
        std::function<void(bool held)> holdRf;
    };

    static StarvationAction actionFor(DSPMode mode);
    /// The sentence the Core stops with.
    static QString stopMessage(const QString& deviceName);

    void setHooks(Hooks hooks) { m_hooks = std::move(hooks); }

    /// `deviceId`'s line starved (true) or carries audio again (false):
    /// holds RF in a Hold mode, releases a hold on audio. Never stops;
    /// returns false.
    bool onStarved(const QByteArray& deviceId, bool starved);
    /// `deviceId`'s line has had no audio packets for kNoAudioCutMs: the
    /// Core stops transmitting, in every mode, and a hold is released.
    /// Returns true when it stopped (false during TUNE or two-tone).
    bool onNoAudio(const QByteArray& deviceId);
    /// Whether the Mic hold is on now.
    bool isHolding() const { return m_holding; }

private:
    void setHolding(bool held);

    Hooks m_hooks;
    bool m_holding{false};
};

} // namespace NereusSDR
