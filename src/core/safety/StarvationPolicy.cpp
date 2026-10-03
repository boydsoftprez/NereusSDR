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
    case DSPMode::RADE_U:
    case DSPMode::RADE_L:
        return StarvationAction::Unkey;
    }
    // A mode this table does not know: the safe answer.
    return StarvationAction::Unkey;
}

QString StarvationPolicy::stopMessage(const QString& deviceName)
{
    return QStringLiteral("No microphone audio arrived from %1, so the Core stopped transmitting.")
        .arg(deviceName);
}

bool StarvationPolicy::onStarved(const QByteArray& deviceId, bool starved)
{
    if (!starved) {
        return false;
    }
    if (m_hooks.microphoneUnused && m_hooks.microphoneUnused()) {
        qCInfo(lcDsp) << "Microphone line from" << deviceId
                      << "starved during TUNE or two-tone; they use no microphone";
        return false;
    }
    const std::optional<DSPMode> mode =
        m_hooks.transmitMode ? m_hooks.transmitMode() : std::nullopt;
    // No transmit mode known: the safe answer, as for an unknown mode.
    const StarvationAction action = mode ? actionFor(*mode) : StarvationAction::Unkey;
    if (action == StarvationAction::KeepKeyed) {
        qCInfo(lcDsp) << "Microphone line from" << deviceId
                      << "starved; staying keyed (silence puts no carrier on the air in mode"
                      << static_cast<int>(*mode) << ")";
        return false;
    }
    const QString name = m_hooks.deviceName ? m_hooks.deviceName(deviceId) : QString();
    const QString message = stopMessage(name.isEmpty() ? QStringLiteral("a device") : name);
    qCWarning(lcDsp).noquote() << "Microphone line from" << QString::fromLatin1(deviceId)
                               << "starved -" << message;
    if (m_hooks.stopAllTx) {
        m_hooks.stopAllTx(message);
    }
    return true;
}

} // namespace NereusSDR
