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
#include <QString>

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

namespace NereusSDR {

namespace {

// Apple's own Mic Mode calls, the ones Control Center's Mic Mode panel
// makes.  Private: AVFCapture exports them (AVFoundation re-exports
// AVFCapture) and no header declares them.  Their types are as the
// AVFCapture disassembly on macOS 27.0 shows: the setter takes the mode
// first and the bundle id second and answers whether it was taken; the
// reader takes the bundle id and answers the mode.  Modes are numbered as
// the public AVCaptureMicrophoneMode numbers them.  The setter throws
// NSInvalidArgumentException for a mode outside the app's supported modes
// (an app that has never used the mic has none; seen on macOS 27.0), so
// both calls are guarded: an exception is a refusal, never a crash.
using ReadMicMode = NSInteger (*)(NSString*);
using WriteMicMode = BOOL (*)(NSInteger, NSString*);
constexpr char kAvFoundationPath[] = "/System/Library/Frameworks/AVFoundation.framework/AVFoundation";
constexpr char kReadSymbol[] = "AVControlCenterMicrophoneModesModuleGetMicrophoneModeForBundleID";
constexpr char kWriteSymbol[] = "AVControlCenterMicrophoneModesModuleSetMicrophoneModeForBundleID";

// The window waits at most this long for the helper's answer, then goes
// on without it.
constexpr int kMicModeWaitMs = 800;

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
        long mode = kMicModeNotRead;
        @autoreleasepool {
            @try {
                mode = static_cast<long>(read(bundleId.toNSString()));
            } @catch (NSException*) {
                mode = kMicModeNotRead;
            }
        }
        return mode;
    };
    calls.write = [write](long mode, const QString& bundleId) {
        bool taken = false;
        @autoreleasepool {
            @try {
                taken = write(static_cast<NSInteger>(mode), bundleId.toNSString()) == YES;
            } @catch (NSException*) {
                taken = false;
            }
        }
        return taken;
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

    const MicModeHelperAnswer answer = runMicModeHelper(helper, bundleId, kMicModeWaitMs);
    const MicModeLogLine line = micModeLogLine(answer, kMicModeWaitMs);
    if (line.text.isEmpty()) {
        qCDebug(lcAudio) << "Mic Mode: Standard for this app, in" << answer.elapsedMs << "ms";
    } else if (line.warning) {
        qCWarning(lcAudio).noquote() << line.text;
    } else {
        qCInfo(lcAudio).noquote() << line.text;
    }
}

} // namespace NereusSDR
