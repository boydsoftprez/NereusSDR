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
// kStarvationMs), which is shorter than the 400 ms link-loss deadline
// (RemoteTxWatchdog). What happens then depends on the transmit mode:
//
//   KeepKeyed: LSB, USB, DSB, CWL, CWU, DIGL, DIGU, SPEC. Silence there
//     puts no carrier on the air, so the transmission goes on (silent)
//     until the operator unkeys or the time-out ends it. DSB is here
//     because WDSP's DSB modulator adds no carrier
//     (third_party/wdsp/src/ammod.c, xammod mode 1, out = mult * in).
//   Unkey: AM, SAM, FM, DRM, RADE_U, RADE_L. Silence there is a bare or
//     garbage carrier, so the Core stops transmitting with "No microphone
//     audio arrived from <device>, so the Core stopped transmitting."
//
// TUNE and two-tone use no microphone and are never stopped by it. A mode
// this table does not know is treated as Unkey.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 37 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
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
    /// Silence is a bare or garbage carrier: stop transmitting.
    Unkey,
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
    };

    static StarvationAction actionFor(DSPMode mode);
    /// The sentence the Core stops with.
    static QString stopMessage(const QString& deviceName);

    void setHooks(Hooks hooks) { m_hooks = std::move(hooks); }

    /// `deviceId`'s line starved (true) or carries audio again (false).
    /// Returns true when the Core stopped transmitting because of it.
    bool onStarved(const QByteArray& deviceId, bool starved);

private:
    Hooks m_hooks;
};

} // namespace NereusSDR
