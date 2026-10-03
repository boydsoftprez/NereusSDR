// =================================================================
// src/gui/setup/CaptureStatusText.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Operator wording for the PC
// microphone capture status; no Thetis logic.  See CaptureStatusText.h.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "gui/setup/CaptureStatusText.h"

namespace NereusSDR {

namespace {

using State = CaptureSupervisor::Status::State;
using Reason = CaptureSupervisor::Status::Reason;

QString defaultMicrophoneName()
{
    return QStringLiteral("the system default microphone");
}

QString failureText(const CaptureSupervisor::Status& status)
{
    switch (status.reason) {
    case Reason::PermissionDenied:
        return QStringLiteral("Microphone access is turned off for NereusSDR. "
                              "Allow it in System Settings, then retry.");
    case Reason::DeviceNotFound:
        if (status.configuredDevice.isEmpty()) {
            return QStringLiteral("The system default microphone is not available.");
        }
        return QStringLiteral("The selected microphone \"%1\" is not available.")
            .arg(status.configuredDevice);
    case Reason::InputLost:
        return QStringLiteral("The microphone stopped sending audio.");
    case Reason::Timeout:
        return QStringLiteral("The microphone did not respond in time.");
    case Reason::HelperMissing:
        return QStringLiteral("Microphone support is missing from this installation.");
    case Reason::HelperDidNotStart:
    case Reason::HelperExited:
    case Reason::ProtocolError:
        return QStringLiteral("Microphone support stopped unexpectedly.");
    case Reason::OpenFailed:
    case Reason::StartFailed:
    case Reason::None:
        break;
    }
    return QStringLiteral("The selected microphone could not be opened.");
}

} // namespace

QString captureStatusText(const CaptureSupervisor::Status& status)
{
    switch (status.state) {
    case State::Closed:
        return QStringLiteral("Microphone not in use");
    case State::PreparingPermission:
        return QStringLiteral("Waiting for microphone permission");
    case State::Opening:
        return QStringLiteral("Preparing microphone");
    case State::Ready: {
        QString name = status.actualDevice;
        if (name.isEmpty()) {
            name = status.configuredDevice;
        }
        if (name.isEmpty()) {
            name = defaultMicrophoneName();
        }
        return QStringLiteral("Microphone ready: %1").arg(name);
    }
    case State::Stopping:
        return QStringLiteral("Stopping microphone");
    case State::Failed:
        return failureText(status);
    }
    return QStringLiteral("Microphone not in use");
}

} // namespace NereusSDR
