// =================================================================
// src/core/audio/MacMicMode.mm  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See MacMicMode.h and MicModeKeeper.h.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#import <Foundation/Foundation.h>

#include "core/audio/MacMicMode.h"

#include "core/LogCategories.h"
#include "core/audio/CaptureHelperLocator.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QString>

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

#include <algorithm>

namespace NereusSDR {

namespace {

// Apple's own Mic Mode calls, the ones Control Center's Mic Mode panel
// makes.  Private: AVFCapture exports them (AVFoundation re-exports
// AVFCapture) and no header declares them.  Their types are as the
// AVFCapture disassembly on macOS 27.0 shows: the setter takes the mode
// first and the bundle id second and answers whether it was taken; the
// reader takes the bundle id and answers the mode.  Modes are numbered as
// the public AVCaptureMicrophoneMode numbers them.
using ReadMicMode = NSInteger (*)(NSString*);
using WriteMicMode = BOOL (*)(NSInteger, NSString*);
constexpr char kAvFoundationPath[] = "/System/Library/Frameworks/AVFoundation.framework/AVFoundation";
constexpr char kReadSymbol[] = "AVControlCenterMicrophoneModesModuleGetMicrophoneModeForBundleID";
constexpr char kWriteSymbol[] = "AVControlCenterMicrophoneModesModuleSetMicrophoneModeForBundleID";

// The window waits at most this long for the helper's answer.
constexpr int kMicModeWaitMs = 1000;

QString mainBundleId()
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return {};
    }
    CFStringRef id = CFBundleGetIdentifier(bundle);
    return id != nullptr ? QString::fromCFString(id) : QString();
}

} // namespace

MicModeCalls systemMicModeCalls()
{
    // Already loaded with AVFoundation; this only finds it.
    void* avFoundation = dlopen(kAvFoundationPath, RTLD_LAZY | RTLD_LOCAL);
    if (avFoundation == nullptr) {
        return {};
    }
    const auto read = reinterpret_cast<ReadMicMode>(dlsym(avFoundation, kReadSymbol));
    const auto write = reinterpret_cast<WriteMicMode>(dlsym(avFoundation, kWriteSymbol));
    if (read == nullptr || write == nullptr) {
        return {};
    }
    MicModeCalls calls;
    calls.read = [read](const QString& bundleId) {
        @autoreleasepool {
            return static_cast<long>(read(bundleId.toNSString()));
        }
    };
    calls.write = [write](long mode, const QString& bundleId) {
        @autoreleasepool {
            return write(static_cast<NSInteger>(mode), bundleId.toNSString()) == YES;
        }
    };
    return calls;
}

void keepWindowMicModeStandard()
{
    const QString bundleId = mainBundleId();
    if (bundleId.isEmpty()) {
        qCInfo(lcAudio) << "Mic Mode: this window has no bundle id, so there is none to keep";
        return;
    }
    const QString helper = locateCaptureHelper(QCoreApplication::applicationDirPath());
    if (helper.isEmpty()) {
        qCWarning(lcAudio) << "Mic Mode: the mic helper is missing; Mic Mode left as it is";
        return;
    }

    QElapsedTimer clock;
    clock.start();
    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(helper, {QString::fromLatin1(kKeepMicModeStandardArg), bundleId});
    const bool finished = process.waitForStarted(kMicModeWaitMs)
                          && process.waitForFinished(
                              std::max(0, kMicModeWaitMs - static_cast<int>(clock.elapsed())));
    if (!finished) {
        process.kill();
        process.waitForFinished(kMicModeWaitMs);
        qCWarning(lcAudio) << "Mic Mode: no answer from the mic helper within" << kMicModeWaitMs
                           << "ms; Mic Mode left as it is";
        return;
    }
    MicModeOutcome outcome = MicModeOutcome::Refused;
    if (process.exitStatus() != QProcess::NormalExit
        || !micModeOutcomeFromExitCode(process.exitCode(), &outcome)) {
        qCWarning(lcAudio) << "Mic Mode: the mic helper ended without an answer, exit code"
                           << process.exitCode();
        return;
    }
    if (outcome == MicModeOutcome::Refused) {
        qCWarning(lcAudio).noquote() << micModeOutcomeText(outcome) << "for this app, in"
                                     << clock.elapsed() << "ms";
        return;
    }
    qCInfo(lcAudio).noquote() << micModeOutcomeText(outcome) << "for this app, in"
                              << clock.elapsed() << "ms";
}

} // namespace NereusSDR
