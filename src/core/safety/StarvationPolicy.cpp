// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/StarvationPolicy.cpp  (NereusSDR)
// =================================================================
//
// See StarvationPolicy.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 37 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-16: Device ids are logged in hex, as the other transmit
//                 logs do; they are binary and printed as raw bytes.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: starvation holds RF in the
//                 carrier modes and audio again releases it; onNoAudio
//                 stops in every mode and releases a hold. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, Task 5: releaseHold.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/safety/StarvationPolicy.h"

#include "core/LogCategories.h"

namespace NereusSDR {

StarvationAction StarvationPolicy::actionFor(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::USB:
    // WDSP's DSB modulator adds no carrier (ammod.c, xammod mode 1), so
    // silence sends nothing.
    case DSPMode::DSB:
    case DSPMode::CWL:
    case DSPMode::CWU:
    case DSPMode::DIGL:
    case DSPMode::DIGU:
    case DSPMode::SPEC:
        return StarvationAction::KeepKeyed;
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::FM:
    case DSPMode::DRM:
    case DSPMode::RADE2_U:
    case DSPMode::RADE_U:
    case DSPMode::RADE2_L:
    case DSPMode::RADE_L:
        return StarvationAction::Hold;
    }
    // A mode this table does not know: the safe answer.
    return StarvationAction::Hold;
}

QString StarvationPolicy::stopMessage(const QString& deviceName)
{
    return QStringLiteral("No microphone audio arrived from %1, so the Core stopped transmitting.")
        .arg(deviceName);
}

bool StarvationPolicy::onStarved(const QByteArray& deviceId, bool starved)
{
    if (!starved) {
        // Audio again: a hold is released at once.
        if (m_holding) {
            qCInfo(lcDsp) << "Microphone line from" << deviceId.toHex().constData()
                          << "carries audio again; releasing the RF hold";
            setHolding(false);
        }
        return false;
    }
    if (m_hooks.microphoneUnused && m_hooks.microphoneUnused()) {
        qCInfo(lcDsp) << "Microphone line from" << deviceId.toHex().constData()
                      << "starved during TUNE or two-tone; they use no microphone";
        return false;
    }
    const std::optional<DSPMode> mode =
        m_hooks.transmitMode ? m_hooks.transmitMode() : std::nullopt;
    // No transmit mode known: the safe answer, as for an unknown mode.
    const StarvationAction action = mode ? actionFor(*mode) : StarvationAction::Hold;
    if (action == StarvationAction::KeepKeyed) {
        qCInfo(lcDsp) << "Microphone line from" << deviceId.toHex().constData()
                      << "starved; staying keyed (silence puts no carrier on the air in mode"
                      << static_cast<int>(*mode) << ")";
        return false;
    }
    qCInfo(lcDsp) << "Microphone line from" << deviceId.toHex().constData()
                  << "starved; holding RF silent while keyed";
    setHolding(true);
    return false;
}

bool StarvationPolicy::onNoAudio(const QByteArray& deviceId)
{
    if (m_hooks.microphoneUnused && m_hooks.microphoneUnused()) {
        qCInfo(lcDsp) << "Microphone line from" << deviceId.toHex().constData()
                      << "sent no audio during TUNE or two-tone; they use no microphone";
        return false;
    }
    const QString name = m_hooks.deviceName ? m_hooks.deviceName(deviceId) : QString();
    const QString message = stopMessage(name.isEmpty() ? QStringLiteral("a device") : name);
    qCWarning(lcDsp).noquote() << "Microphone line from" << deviceId.toHex().constData()
                               << "sent no audio -" << message;
    // The stop first, then the hold's release, so RF is never let go while
    // still keyed.
    if (m_hooks.stopAllTx) {
        m_hooks.stopAllTx(message);
    }
    if (m_holding) {
        setHolding(false);
    }
    return true;
}

void StarvationPolicy::releaseHold()
{
    if (!m_holding) {
        return;
    }
    qCInfo(lcDsp) << "Microphone hold released: its key ended, or TUNE or two-tone started";
    setHolding(false);
}

void StarvationPolicy::setHolding(bool held)
{
    m_holding = held;
    if (m_hooks.holdRf) {
        m_hooks.holdRf(held);
    }
}

} // namespace NereusSDR
