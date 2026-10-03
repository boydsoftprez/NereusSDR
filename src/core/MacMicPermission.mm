#import <AVFoundation/AVFoundation.h>

#include "core/LogCategories.h"
#include "core/MacMicPermission.h"
#include <QLoggingCategory>

namespace NereusSDR {

void requestMicrophonePermission()
{
    AVAuthorizationStatus status =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

    const char* statusName = "unknown";
    switch (status) {
    case AVAuthorizationStatusNotDetermined: statusName = "NotDetermined"; break;
    case AVAuthorizationStatusRestricted:    statusName = "Restricted";    break;
    case AVAuthorizationStatusDenied:        statusName = "Denied";        break;
    case AVAuthorizationStatusAuthorized:    statusName = "Authorized";    break;
    }
    qCInfo(lcAudio) << "Microphone TCC status on launch:" << statusName;

    switch (status) {
    case AVAuthorizationStatusNotDetermined:
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                 completionHandler:^(BOOL granted) {
            if (granted) {
                qCInfo(lcAudio) << "Microphone access granted";
            } else {
                qCWarning(lcAudio) << "Microphone access denied by user";
            }
        }];
        break;

    case AVAuthorizationStatusAuthorized:
        break;

    case AVAuthorizationStatusDenied:
    case AVAuthorizationStatusRestricted:
        qCWarning(lcAudio)
            << "Microphone access denied/restricted. Open System Settings → "
               "Privacy & Security → Microphone and enable access for NereusSDR.";
        break;
    }
}

namespace {

MicPermission toMicPermission(AVAuthorizationStatus status)
{
    switch (status) {
    case AVAuthorizationStatusAuthorized:    return MicPermission::Granted;
    case AVAuthorizationStatusNotDetermined: return MicPermission::Undetermined;
    case AVAuthorizationStatusDenied:
    case AVAuthorizationStatusRestricted:    return MicPermission::Denied;
    }
    return MicPermission::Denied;
}

} // namespace

MicPermission microphonePermissionStatus()
{
    return toMicPermission(
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]);
}

MicPermission requestMicrophonePermissionAndWait()
{
    const MicPermission current = microphonePermissionStatus();
    if (current != MicPermission::Undetermined) {
        return current;
    }
    // The completion handler runs on an arbitrary dispatch queue, never
    // the calling thread, so waiting here cannot deadlock it.
    __block BOOL answer = NO;
    dispatch_semaphore_t answered = dispatch_semaphore_create(0);
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted) {
        answer = granted;
        dispatch_semaphore_signal(answered);
    }];
    dispatch_semaphore_wait(answered, DISPATCH_TIME_FOREVER);
#if !__has_feature(objc_arc)
    dispatch_release(answered);
#endif
    qCInfo(lcAudio) << "Microphone permission answered:"
                    << (answer ? "granted" : "denied");
    return answer ? MicPermission::Granted : MicPermission::Denied;
}

} // namespace NereusSDR
