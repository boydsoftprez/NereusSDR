// no-port-check: NereusSDR-original. See header.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/OperatorReasonText.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for full
// Modification history (NereusSDR).
// =================================================================

#include "gui/OperatorReasonText.h"

#include "core/session/media/SpectrumEndpoint.h"

#include <QLatin1String>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>

#include <vector>

Q_LOGGING_CATEGORY(lcOperatorReason, "nereus.remote.reason")

namespace NereusSDR::OperatorReasonText {
namespace {

struct Entry {
    const char* wire;       // Byte-for-byte as sent or recorded; never reworded.
    const char* shortLine;  // A pan's short line; nullptr where no pan shows it.
    const char* sentence;   // Why, in user words.
    const char* shorter = nullptr;  // A shorter short line, for a narrow pan.
    const char* panNext = nullptr;  // What happens next on a pan, when not the default.
};

// The left column is copied from where each reason is written, so a match
// is exact. Keep it in step with those sites; the wording on the right is
// what the user reads. A reason that is already in user words needs no
// entry: forDisplay() shows it as sent.
constexpr Entry kEntries[] = {
    // The Core's display refusals, DaemonMediaController.cpp.
    {"The Core's display limit has no room left.", "Refused: Core busy",
     "The Core's display limit has no room left for this pan.", "Refused"},
    {"The Core is already sending as many displays as it can.", "Refused: pan limit",
     "The Core is already sending as many pan displays as it can.", "Refused"},
    {"This display had already been closed.", "Refused: out of date",
     "The Core had already closed the display this request was for.", "Refused"},
    {"A newer request for this display had already arrived.", "Refused: out of date",
     "A newer request for this pan had already reached the Core.", "Refused"},
    {"The receiver's spectrum settings changed first.", "Refused: out of date",
     "The receiver's spectrum settings changed before the Core could answer.", "Refused"},
    {"The Core could not read this display request.", "Refused: bad request",
     "The Core could not read this pan's display request.", "Refused"},
    {"The Core could not read the request to stop this display.", "Refused: bad request",
     "The Core could not read the request to stop this pan's display.", "Refused"},
    {"This view reaches past what the receiver covers.", "Refused: out of range",
     "This view reaches past the frequencies the receiver covers.", "Refused"},
    {kRetireReasonSourceRetune, "Refused: out of range",
     "The receiver was retuned and no longer covers this view.", "Refused"},
    {"This display's receiver is not on the Core.", "Refused: no slice",
     "This pan's slice is not available on the Core.", "Refused"},
    // iPhone app Task 76 (ruling 9.1): displays only for a device's own slices.
    {"That slice belongs to another device.", "Refused: other device",
     "This pan's slice belongs to another device on the Core.", "Refused"},
    {kRetireReasonSliceRemoved, "Refused: no slice",
     "This pan's slice was removed.", "Refused"},
    {kRetireReasonStreamBindingChanged, "Refused: out of date",
     "This pan's slice moved to a different receiver.", "Refused"},
    {"The Core could not set up this receiver's spectrum.", "Refused: setup failed",
     "The Core could not set up the spectrum for this receiver.", "Refused"},
    {"This receiver's spectrum stopped on the Core.", "Refused: not ready",
     "The spectrum for this receiver stopped on the Core.", "Refused"},
    {"The receiver's spectrum is not ready.", "Refused: not ready",
     "The Core does not have this receiver's spectrum ready.", "Refused"},
    {"The extended view is not available right now.", "Refused: no wide view",
     "The Core cannot provide the extended view right now.", "Refused"},
    {"The Core stopped its display service.", "Refused: Core stopping",
     "The Core was stopping its display service.", "Refused"},
    {"The Core has no room left for the PureSignal display.", "Refused: Core busy",
     "The Core's display limit has no room for the PureSignal display.", "Refused"},
    {"This display's spectrum was closed.", "Refused: out of date",
     "The Core had already closed this pan's display.", "Refused"},
    {"The Core stopped sending displays.", "Refused: Core stopping",
     "The Core had stopped sending pan displays.", "Refused"},

    // Recorded by this computer, RemoteMediaController.cpp.
    {"Core refused the display allocation.", "Refused by the Core",
     "The Core did not accept this pan's display request.", "Refused"},
    {"Core refused the display release.", "Refused by the Core",
     "The Core did not accept the request to stop this pan's display.", "Refused"},
    {"Unable to send the allocation request.", "Request not sent",
     "This computer could not send the request to the Core.", "Not sent"},
    {"Unable to request PureSignal display release.", "Request not sent",
     "This computer could not send the PureSignal display request to the Core.", "Not sent"},
    {"Unable to request PureSignal display admission.", "Request not sent",
     "This computer could not send the PureSignal display request to the Core.", "Not sent"},

    // Recorded by this computer, RemoteDisplayAllocator.cpp. This app
    // decided before asking the Core, and decides again whenever the Core's
    // limits or the open pans change, so each says what changes it.
    {"Display budget limits are invalid.", "Core limits unusable",
     "The Core reported display limits this app cannot use.", "Can't show",
     "This app tries again when the Core reports new limits."},
    {"At most eight remote display pans are supported.", "Too many pans",
     "This app shows the Core's display on up to eight pans at once.", nullptr,
     "Close a pan to show this one."},
    {"Remote display pan IDs must be unique.", "Can't show this pan",
     "Two pans could not be told apart.", "Can't show",
     "This app tries again when a pan opens, closes or changes."},
    {"At most one remote display pan may be active.", "Can't show this pan",
     "More than one pan was marked as the active pan.", "Can't show",
     "This app tries again when a pan opens, closes or changes."},
    {"PureSignal display reservation does not fit the display budget.", "Refused: Core busy",
     "The Core's display limit has no room for the PureSignal display.", "Refused",
     "Close a pan or make one smaller to make room."},
    {"Display budget cannot reserve requested display demand.", "Refused: Core busy",
     "The Core's display limit has no room for the pan displays you have open.", "Refused",
     "Close a pan or make one smaller to show this one."},

    // Why the link to the Core ended: the Core's own reasons
    // (StationServer.cpp) and this computer's (StationClient.cpp).
    {"The Core already has as many connections as it allows. Try again shortly.", nullptr,
     "The Core already has as many connections as it allows. This app tries again shortly."},
    {"heartbeat timeout", nullptr,
     "The connection to the Core went quiet, so it was closed."},
    {"This app stopped answering, so the Core closed the connection.", nullptr,
     "The connection to the Core went quiet, so it was closed."},
    {"The Core is refusing pairing tokens for a while after too many wrong ones. Try again later.", nullptr,
     "The Core is refusing pairing tokens for a while after too many wrong ones. "
     "This app tries again shortly."},
    {"replaced by a newer session", nullptr,
     "This app opened a new connection to the Core in place of this one."},
    {"link closed", nullptr,
     "The connection to the Core closed."},

    // Why a receiver's audio for an app on this computer (TCI, VAX) is not
    // coming (R-R3-43): the receiver audio context's wire reasons,
    // RemoteAudioContext.cpp. The speakers' own status words these in
    // RemoteAudioStatus.cpp.
    {"client-disabled", nullptr,
     "This app stopped asking for this receiver's audio."},
    {"media-not-ready", nullptr,
     "Audio from the Core is not ready. This app asks for it again when it is."},
    {"radio-offline", nullptr,
     "The radio at the Core is offline. This receiver's audio comes back with the radio."},
    {"encoder-unavailable", nullptr,
     "The Core could not start this receiver's audio."},
    {"slice-removed", nullptr,
     "This receiver is no longer on the Core."},
    {"receiver-limit", nullptr,
     "The Core is already sending audio for as many receivers as it can. Stop the audio "
     "for another receiver to hear this one."},
    // R-R3-45: the headphones audio context's own reason.
    {"no-headphones-receiver", nullptr,
     "No receiver is playing on the headphones."},

    // Audio and display from the Core: the Core's refusals
    // (DaemonMediaController::sendRejected) and this computer's own
    // (RemoteMediaController.cpp, MediaPeer.cpp).
    {"Station media connection closed", nullptr,
     "The audio and display connection to the Core closed. This app reconnects it."},
    {"media transport factory failed", nullptr,
     "This computer could not set up audio and display."},
    {"media transport factory returned an invalid object", nullptr,
     "This computer could not set up audio and display."},
    {"invalid local media description rejected", nullptr,
     "Audio and display stopped because a setup message could not be used."},
    {"invalid local media candidate rejected", nullptr,
     "Audio and display stopped because a setup message could not be used."},
    {"buffered media candidate rejected", nullptr,
     "Audio and display stopped because a setup message could not be used."},
    {"invalid display message rejected", nullptr,
     "Audio and display stopped because a display message from the Core could not be used."},
    {"invalid raw RTP packet rejected", nullptr,
     "Audio and display stopped because an audio packet from the Core could not be used."},
    // The media transport's own reasons, LibDataChannelMediaTransport.cpp.
    // A failed connection is redialled (RemoteMediaController::
    // requestRecovery, then RemoteConnectionController::recoverMediaSession,
    // whose fallback reason is the last entry); every other one stops audio
    // and display for this connection (settleWithoutRetry).
    {"media peer connection failed", nullptr,
     "The audio and display connection to the Core failed. This app reconnects to the Core."},
    {"station media connection failed", nullptr,
     "The audio and display connection to the Core failed. This app reconnects to the Core."},
    {"media callback queue overflow", nullptr,
     "Audio and display stopped because this computer fell behind handling them."},
    {"oversized display message rejected", nullptr,
     "Audio and display stopped because a display message from the Core could not be used."},
    {"text display message rejected", nullptr,
     "Audio and display stopped because a display message from the Core could not be used."},
    {"invalid raw RTP packet size rejected", nullptr,
     "Audio and display stopped because an audio packet from the Core could not be used."},
    {"text RTP message rejected", nullptr,
     "Audio and display stopped because an audio packet from the Core could not be used."},
    {"oversized local description rejected", nullptr,
     "Audio and display stopped because a setup message could not be used."},
    {"oversized local candidate rejected", nullptr,
     "Audio and display stopped because a setup message could not be used."},
    {"unexpected display data channel rejected", nullptr,
     "Audio and display stopped because the Core set up the display in a way this app does "
     "not use. Updating this app or the Core may help."},
    {"unexpected media track rejected", nullptr,
     "Audio and display stopped because the Core set up the audio in a way this app does "
     "not use. Updating this app or the Core may help."},

    // Requests the Core turned down: StationServer.cpp (an app too old for
    // the request) and SessionCommandDispatcher.cpp.
    {"Update this app to set up the Tuner Genius XL on this Core.", nullptr,
     "Update this app to set up the Tuner Genius on this Core."},

    // Settings the Core kept as they were, StationServer.cpp and
    // SettingsScope.cpp.
    {"This change named the same setting twice.", nullptr,
     "The Core could not read this change."},
    {"The Core changes these settings only through their own controls.", nullptr,
     "Change this with the model controls; the Core checks every model change."},

    // This computer's own refusals, StationClient.cpp and
    // PureSignalSessionFacade.cpp.
    {"This station session does not support NNR controls.", nullptr,
     "This Core does not offer NNR controls to this app."},
    {"This station session does not support NNR.", nullptr,
     "This Core does not offer NNR to this app."},
    {"There is no station session.", nullptr,
     "This app is not connected to the Core."},
    // StationClient.cpp: a control this Core has not offered this app, or
    // not yet (these are also refused before the Core has answered).
    {"The station does not support PS3 settings.", nullptr,
     "This Core does not offer PureSignal settings to this app."},
    {"The station does not support remote C-Tune.", nullptr,
     "This Core does not offer C-Tune to this app."},
    {"The station does not support remote TGXL configuration.", nullptr,
     "This Core does not offer Tuner Genius XL setup to this app."},
    {"The station does not support remote PGXL configuration.", nullptr,
     "This Core does not offer Power Genius XL setup to this app."},
    {"The station does not support NNR model application.", nullptr,
     "This Core does not offer NNR model changes to this app."},
    {"The station does not support NNR diagnostics.", nullptr,
     "This Core does not offer NNR diagnostics to this app."},
    {"The station does not support remote 4O3A control.", nullptr,
     "This Core does not offer 4O3A control to this app."},
    {"This station cannot try noise reduction again. Update the station software.", nullptr,
     "This Core cannot try noise reduction again. Update the Core."},

    // Noise reduction status and refusals, NnrAdapter.cpp (two also in
    // SliceModel.cpp and RadioModel.cpp): shown in the NNR panel and when
    // turning NNR on is refused.
    {"NNR cannot run at this receiver's processing rate. Use another noise reduction, or change the processing rate.", nullptr,
     "NNR cannot run at this receiver's processing rate, which must be a whole multiple of "
     "the NNR model's rate. Use another noise reduction, or change the processing rate."},
    {"NNR receiver is not available.", nullptr,
     "NNR is not available on this receiver."},
    {"The selected NNR model is unavailable.", nullptr,
     "The chosen NNR model is not available, so NNR cannot run."},
    {"NNR is not included in this build.", nullptr,
     "NNR is not included in the NereusSDR software running this receiver."},
    {"NNR values must be finite and within their supported ranges.", nullptr,
     "One of the NNR settings is out of range, so nothing was changed."},
    {"The requested NNR model is not ready; no tuning was changed.", nullptr,
     "The chosen NNR model is not ready, so the NNR settings were not changed."},
    {"Unsupported NNR diagnostic mode.", nullptr,
     "That NNR diagnostic setting is not available."},
    {"The NNR receiver is not ready.", nullptr,
     "NNR is not ready on this receiver."},

    // The Core's certificate, StationClient.cpp: shown after "Last failure:"
    // and in the link-lost toast. "Certificate fingerprint" is the one name
    // the Core setup window and the Connections window use for it.
    {"No station certificate fingerprint to pin. Refusing to connect: an "
     "unpinned self-signed certificate authenticates nothing.", nullptr,
     "No certificate fingerprint is saved for this Core, so this app did not connect. "
     "Copy the fingerprint from the Core's setup into this Core's entry."},
    {"Refusing to authenticate: a station certificate fingerprint is pinned, but this "
     "link presented no certificate to compare it against.", nullptr,
     "The Core did not show a certificate, so this app could not check it against the "
     "saved certificate fingerprint and did not connect."},
    {"Station certificate fingerprint does not match the saved pin.", nullptr,
     "The Core's certificate does not match the certificate fingerprint saved for it, so "
     "this app did not connect. If the Core was set up again, save its new fingerprint "
     "from the Core's setup."},

    // LAN discovery, StationLanCache.cpp, StationLanAnnouncement.cpp and
    // StationLanDiscovery.cpp: the Connections window's discovery line.
    {"Station LAN announcement has an invalid source address.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an unscoped link-local source.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid control port.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid fingerprint.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid Core name.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid radio name.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid radio MAC.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid radio connection state.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    // iPhone app Task 16: the schema-2 fields.
    {"Station LAN announcement has an invalid claimed state.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid identity.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid label.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an invalid pairing state.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    // iPhone app Task 71: the device count appended to schema 2.
    {"Station LAN announcement has an invalid device count.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    // iPhone app plan Task 25: the radio state appended after the count.
    {"Station LAN announcement has an invalid radio state.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement schema 1 cannot carry the Core's identity.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement is too large.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement is malformed.", nullptr,
     "A Core announcement on this network could not be read, so that Core is not listed."},
    {"Station LAN announcement has an unsupported schema.", nullptr,
     "A Core on this network announced itself in a form this app does not know, so it is "
     "not listed. Updating this app may help."},
    {"Station LAN announcement has an unsupported service.", nullptr,
     "A Core on this network announced itself in a form this app does not know, so it is "
     "not listed. Updating this app may help."},
    {"Station LAN announcement cache is full.", nullptr,
     "More Cores are announcing on this network than this app lists, so some are missing. "
     "Add a missing Core by its address."},
    {"Station LAN announcement endpoint limit reached.", nullptr,
     "More Cores are announcing on this network than this app lists, so some are missing. "
     "Add a missing Core by its address."},
    {"Station LAN discovery could not bind a UDP socket.", nullptr,
     "This app could not listen for Cores on this network. Saved and typed addresses still work."},
    {"Station LAN discovery could not read a UDP datagram.", nullptr,
     "This app could not read an announcement on this network."},
    {"Station LAN discovery ignored an oversized datagram.", nullptr,
     "This app ignored an announcement on this network that was too large to come from a Core."},
    {"Station LAN discovery IPv4 is unavailable.", nullptr,
     "This app cannot listen for Cores on IPv4 networks."},
    {"Station LAN discovery IPv6 is unavailable.", nullptr,
     "This app cannot listen for Cores on IPv6 networks."},
    {"Station LAN discovery has no eligible multicast interface.", nullptr,
     "This computer has no network this app can listen for Cores on."},
    {"Station LAN discovery could not join a multicast group.", nullptr,
     "This app could not listen for Cores on every network."},
};

// Reasons only an older Core sends (R-IOS-01): the Core now sends each in
// operator words, and this app shows those as the entries above say, or as
// sent when no entry is needed. An older Core still sends these, so each is
// shown in the same words as before. None is written in the sources any
// more (tst_operator_wording_sweep checks both ways).
constexpr Entry kOlderCoreEntries[] = {
    // An older Core words these with "yet"; this app shows them without it.
    {"The receiver's spectrum is not ready yet.", "Refused: not ready",
     "The Core does not have this receiver's spectrum ready.", "Refused"},
    {"CW transmit is not available yet", nullptr,
     "CW transmit is not available on this Core"},
    {"FM transmit is not available yet", nullptr,
     "FM transmit is not available on this Core"},
    {"DRM transmit is not available yet", nullptr,
     "DRM transmit is not available on this Core"},
    {"This device's microphone is not connected to the Core yet. Wait a moment and try again.",
     nullptr,
     "This device's microphone is not connected to the Core. Wait a moment and try again."},
    {"No sound has reached the Core from this device's microphone yet. "
     "Wait a moment and try again.", nullptr,
     "No sound has reached the Core from this device's microphone. "
     "Wait a moment and try again."},
    {"PureSignal cannot be run from a remote window yet.", nullptr,
     "PureSignal cannot be run from a remote window."},
    {"session display budget exceeded", "Refused: Core busy",
     "The Core's display limit has no room left for this pan.", "Refused"},
    {"endpoint limit reached", "Refused: pan limit",
     "The Core is already sending as many pan displays as it can.", "Refused"},
    {"endpoint identifier retired", "Refused: out of date",
     "The Core had already closed the display this request was for.", "Refused"},
    {"stale revision", "Refused: out of date",
     "A newer request for this pan had already reached the Core.", "Refused"},
    {"incompatible source window", "Refused: out of date",
     "The receiver's spectrum settings changed before the Core could answer.", "Refused"},
    {"invalid subscription", "Refused: bad request",
     "The Core could not read this pan's display request.", "Refused"},
    {"invalid unsubscription", "Refused: bad request",
     "The Core could not read the request to stop this pan's display.", "Refused"},
    {"requested crop is outside source coverage", "Refused: out of range",
     "This view reaches past the frequencies the receiver covers.", "Refused"},
    {"slice is unavailable", "Refused: no slice",
     "This pan's slice is not available on the Core.", "Refused"},
    {"source configuration rejected", "Refused: setup failed",
     "The Core could not set up the spectrum for this receiver.", "Refused"},
    {"source configuration became unavailable", "Refused: not ready",
     "The spectrum for this receiver stopped on the Core.", "Refused"},
    {"source geometry is unavailable", "Refused: not ready",
     "The Core does not have this receiver's spectrum ready.", "Refused"},
    {"wideband source is unavailable", "Refused: no wide view",
     "The Core cannot provide the extended view right now.", "Refused"},
    {"display budget authority retired", "Refused: Core stopping",
     "The Core was stopping its display service.", "Refused"},
    {"PureSignal display does not fit the session display budget", "Refused: Core busy",
     "The Core's display limit has no room for the PureSignal display.", "Refused"},
    {"display source retired", "Refused: out of date",
     "The Core had already closed this pan's display.", "Refused"},
    {"display production retired", "Refused: Core stopping",
     "The Core had stopped sending pan displays.", "Refused"},
    {"Station is at its concurrent-connection limit", nullptr,
     "The Core already has as many connections as it allows. This app tries again shortly."},
    {"Too many failed authentication attempts; try again later", nullptr,
     "The Core is refusing pairing tokens for a while after too many wrong ones. "
     "This app tries again shortly."},
    {"Remote TGXL configuration requires a newer station protocol.", nullptr,
     "Update this app to set up the Tuner Genius on this Core."},
    {"Duplicate property in one write.", nullptr,
     "The Core could not read this change."},
    {"Use the station DSP controls; raw settings writes cannot bypass model validation.", nullptr,
     "Change this with the model controls; the Core checks every model change."},
    {"NNR requires a DSP rate that is an integer multiple of its network rate.", nullptr,
     "NNR cannot run at this receiver's processing rate, which must be a whole multiple of "
     "the NNR model's rate. Use another noise reduction, or change the processing rate."},
    {"station shutting down", nullptr,
     "The Core is shutting down."},
    {"handshake deadline expired", nullptr,
     "This app did not finish connecting to the Core in time."},
    {"undecodable message", nullptr,
     "The Core could not read a message from this app."},
    {"message sent before authentication", nullptr,
     "This app sent a request before the Core had accepted its pairing token."},
    {"duplicate hello", nullptr,
     "This app started connecting twice on one connection."},
    {"auth before hello", nullptr,
     "This app sent its pairing token out of order."},
    {"duplicate auth", nullptr,
     "This app sent its pairing token out of order."},
    {"Authentication failed", nullptr,
     "The Core did not accept this app's pairing token. Check the token saved for this Core."},
    {"Remote 4O3A control requires a newer station protocol.", nullptr,
     "Update this app to control 4O3A on this Core."},
    {"This DSP action requires a newer station protocol.", nullptr,
     "Update this app to use this control on this Core."},
    {"Remote C-Tune requires a newer station protocol.", nullptr,
     "Update this app to use C-Tune on this Core."},
    {"no radio model attached", nullptr,
     "The Core has no radio ready."},
    {"Malformed DSP asset request.", nullptr,
     "The Core could not read this request."},
    {"Model application requires the current selection revision.", nullptr,
     "The model choice changed on the Core before this request arrived. Try again."},
    {"unrecognised command verb", nullptr,
     "The Core does not know this request. Updating the Core may help."},
    {"Malformed PureSignal request or retired session.", nullptr,
     "The Core could not use this PureSignal request."},
    {"A display subscription requires one boolean enabled value.", nullptr,
     "The Core could not read the PureSignal display request."},
    {"Unknown PureSignal action.", nullptr,
     "The Core does not know this PureSignal action."},
    {"Remote PureSignal actuation requires R4 transmit support.", nullptr,
     "PureSignal cannot be run from a remote window."},
    {"This PureSignal command identity is already pending.", nullptr,
     "This PureSignal request is already in progress."},
    {"Unknown receiver or invalid NNR action arguments.", nullptr,
     "The Core could not apply this NNR change to that receiver."},
    {"no such slice", nullptr,
     "That receiver is no longer on the Core."},
    {"rejected by the slice allocator", nullptr,
     "The Core could not add another receiver."},
    {"cannot remove the last remaining slice", nullptr,
     "The last receiver cannot be removed."},
    {"slice is not bound to an active stream", nullptr,
     "That receiver is not running on the Core."},
    {"C-Tune centre is invalid for this stream's cohosts", nullptr,
     "C-Tune cannot center there while other receivers share this spectrum."},
    {"disconnectTgxl takes no arguments", nullptr,
     "The Core could not read this request."},
    {"setFourO3AEnabled requires exactly one enabled boolean argument", nullptr,
     "The Core could not read this request."},
    {"no such mirrored property", nullptr,
     "The Core does not have this setting."},
    {"Unknown property or incompatible value type.", nullptr,
     "The Core does not have this setting, or not in this form."},
    {"This client has not negotiated DSP settings control.", nullptr,
     "Update this app to change these settings on this Core."},
    {"The station retained the returned value after validating this edit.", nullptr,
     "The Core checked this change and kept the value shown."},
    {"Use the validated DSP controls to change these settings.", nullptr,
     "Change these settings with their own controls on this Core."},
    {"DSP asset actions are station-local.", nullptr,
     "The Core could not read this request."},
    {"DSP asset action owner is required.", nullptr,
     "The Core could not read this request."},
    {"Advertised DSP asset size is outside the format limit.", nullptr,
     "The file is too large for this kind of model or correction."},
    {"Advertised DSP asset hash must be 64 hexadecimal characters.", nullptr,
     "The Core could not read this request."},
    {"At most four DSP asset imports may be active.", nullptr,
     "The Core is already importing as many files as it can. Try again when one finishes."},
    {"At most two DSP asset imports may be active for one owner.", nullptr,
     "This app is already importing as many files as it can. Try again when one finishes."},
    {"DSP import transfer is missing, retired, or belongs to another owner.", nullptr,
     "The import was lost on the Core. Start it again."},
    {"DSP import chunk offset is not the next expected offset.", nullptr,
     "The import was interrupted. Start it again."},
    {"DSP import chunk data is not canonical bounded base64.", nullptr,
     "The import was interrupted. Start it again."},
    {"DSP import exceeds its advertised size.", nullptr,
     "The import was interrupted. Start it again."},
    {"DSP import transfer ID is invalid.", nullptr,
     "The import was interrupted. Start it again."},
    {"DSP import size or SHA-256 does not match its advertisement.", nullptr,
     "The file did not reach the Core intact. Start the import again."},
    {"DSP asset export fields are invalid.", nullptr,
     "The Core could not read this request."},
    {"DSP asset ID is not present in this station store.", nullptr,
     "That file is no longer on the Core."},
    {"DSP asset export offset exceeds its size.", nullptr,
     "The export was interrupted. Start it again."},
    {"Could not read the validated DSP asset.", nullptr,
     "The Core could not read that file."},
    {"Unknown DSP asset action.", nullptr,
     "The Core does not know this request."},
    {"NNR model selection fields are invalid.", nullptr,
     "The Core could not read this model choice."},
    {"NR3 model selection fields are invalid.", nullptr,
     "The Core could not read this model choice."},
    {"Selected NNR model asset is unavailable.", nullptr,
     "The chosen NNR model is not on the Core."},
    {"The station session retired this file operation.", nullptr,
     "The connection to the Core changed, so this file operation stopped."},
    {"media peer already active", nullptr,
     "The Core is already sending audio and display on another connection."},
    {"media peer start failed", nullptr,
     "The Core could not start audio and display."},
    {"Connect Core to a radio before configuring its PGXL.", nullptr,
     "Connect the Core to a radio before setting up its Power Genius."},
    {"Enable 4O3A on Core before connecting the PGXL.", nullptr,
     "Turn on 4O3A on the Core before connecting the Power Genius."},
    {"Enter a valid PGXL IP address or hostname and TCP port 1 to 65535.", nullptr,
     "Enter the Power Genius's IP address or host name, and a port from 1 to 65535."},
    {"source retune no longer covers requested crop", "Refused: out of range",
     "The receiver was retuned and no longer covers this view.", "Refused"},
    // The Core's Tuner Genius and Power Genius checks before the Core sent
    // them in operator words (iPhone app Part A fix wave): shown on the
    // Peripherals page's rows. The worded ones are patterns below.
    {"TGXL identity was rejected by station discovery", nullptr,
     "The Core could not confirm that the device at this address is a Tuner Genius."},
    {"TGXL native info omitted a nonempty serial", nullptr,
     "The Tuner Genius at this address did not say which unit it is."},
    {"TGXL native identity timed out", nullptr,
     "The device at this address did not answer as a Tuner Genius in time."},
    {"PGXL identity was rejected by station discovery", nullptr,
     "The Core could not confirm that the device at this address is a Power Genius."},
    {"PGXL native info omitted a nonempty serial", nullptr,
     "The Power Genius at this address did not say which unit it is."},
    {"PGXL native identity timed out", nullptr,
     "The device at this address did not answer as a Power Genius in time."},
    // The Core's reasons before they called the Core "the Core" (R-R3-21,
    // operator decision of 2026-09-24): StationServer.cpp,
    // SessionCommandDispatcher.cpp, AmplifierModel.cpp and WdspEngine.cpp.
    {"Transmit configuration is unavailable on this receive-only station.", nullptr,
     "Transmit configuration is unavailable on this receive-only Core."},
    {"The station sets this itself; it cannot be changed from here.", nullptr,
     "The Core sets this itself; it cannot be changed from here."},
    {"Update this app to turn the station's TCI server on or off.", nullptr,
     "Update this app to turn the Core's TCI server on or off."},
    {"This Core has no TCI server for the station.", nullptr,
     "This Core has no TCI server."},
    {"The request to turn the station's TCI server on or off was not understood.", nullptr,
     "The request to turn the Core's TCI server on or off was not understood."},
    {"Operating the station's amplifier or tuner waits for remote transmit. This station is "
     "receive-only.", nullptr,
     "Operating the station's amplifier or tuner waits for remote transmit. This Core is "
     "receive-only."},
    {"NNR model changes apply after disconnecting and reconnecting the station.", nullptr,
     "NNR model changes apply after the radio is disconnected and connected again."},
};

// Reasons worded around a value (a number, a pan's name, an action). The
// sentence takes capture 1 as %1 when it has one, and only a capture that
// is itself in user words; `sample` is one real instance, for tests.
struct Pattern {
    const char* regex;
    const char* shortLine;
    const char* sentence;
    const char* sample;
    const char* shorter = nullptr;
    const char* panNext = nullptr;
};

const Pattern kPatterns[] = {
    // RemoteDisplayAllocator.cpp words this one around the pan's name.
    {R"(^Remote display intent '.*' has an invalid range\.$)", "Can't show this pan",
     "This pan's size or update rate is out of range.",
     "Remote display intent 'pan' has an invalid range.", "Can't show",
     "This app tries again when a pan opens, closes or changes."},

    // The version check, from either end (StationServer.cpp and
    // StationClient.cpp).
    {R"(^Protocol major version mismatch: )", nullptr,
     "This app and the Core are too far apart in version to work together. "
     "Update the older one.",
     "Protocol major version mismatch: station speaks 2.9, client speaks 1.4. "
     "A differing major means an incompatible wire contract."},
    {R"(^Displaced by a newer authenticated connection from )", nullptr,
     "Another app connected to the Core and took over. Connect again to take it back.",
     "Displaced by a newer authenticated connection from 192.0.2.7:50123"},
    // R-IOS-01: the Core's own words for it now name the other app's
    // address; this app shows the same sentence as before.
    {R"(^Another app at .+ connected to the Core and took over\. Connect again to take it back\.$)",
     nullptr,
     "Another app connected to the Core and took over. Connect again to take it back.",
     "Another app at 192.0.2.7:50123 connected to the Core and took over. Connect again to "
     "take it back."},
    // Qt's own words for a secure connection that could not be set up.
    {R"((?i)handshake)", nullptr,
     "A secure connection to the Core could not be set up.",
     "SSL handshake failed"},

    // RemoteMediaController.cpp: the two stages of starting audio and
    // display, each with its deadline in seconds.
    {R"(^Core sent no station media description within ([0-9.]+) seconds$)", nullptr,
     "The Core did not start audio and display within %1 seconds. This app tries again.",
     "Core sent no station media description within 5 seconds"},
    {R"(^Station media did not connect within ([0-9.]+) seconds$)", nullptr,
     "Audio and display from the Core did not connect within %1 seconds. "
     "This app tries again.",
     "Station media did not connect within 1.5 seconds"},

    // SessionCommandDispatcher.cpp: a request the Core could not read.
    {R"(^(missing|invalid) [A-Za-z ]+ argument$)", nullptr,
     "The Core could not read this request.",
     "missing sliceId argument"},
    {R"(^[A-Za-z]+ argument is not a whole number this station can represent$)", nullptr,
     "The Core could not use one of the values in this request.",
     "rateHz argument is not a whole number this station can represent"},

    // DspAssetService.cpp: a model or correction request the Core could
    // not read.
    {R"(^dspAssets\.[A-Za-z0-9]+ (has invalid or missing fields|field types or ranges are invalid|accepts no arguments)\.$)",
     nullptr, "The Core could not read this request.",
     "dspAssets.selectNr3Model has invalid or missing fields."},

    // StationClient.cpp: a saved certificate fingerprint on an address with
    // no secure connection to check it on.
    {R"(^Refusing to connect to .*: a station certificate fingerprint is pinned, but )",
     nullptr,
     "This Core's address does not use a secure connection, so this app could not check "
     "the saved certificate fingerprint and did not connect. Use an address that starts "
     "with wss://.",
     "Refusing to connect to ws://192.0.2.7:4433: a station certificate fingerprint is "
     "pinned, but \"ws\" carries no TLS, so there is nothing to compare the fingerprint "
     "against and the pairing token would travel in cleartext. Use wss://."},

    // The Core's Tuner Genius XL checks, StationTgxlController.cpp and
    // TgxlConnection.cpp, each worded around a model, address or serial.
    {R"(^Expected TunerGenius/TunerGeniusXL at the connected endpoint; observed .* \(serial .*\)\.$)",
     nullptr,
     "The device at this address is not a Tuner Genius. Check the tuner's address and port.",
     "Expected TunerGenius/TunerGeniusXL at the connected endpoint; observed PowerGeniusXL "
     "(serial 1234-5678)."},
    {R"(^No matching TGXL discovery announcement for .+\. Check the tuner address, port and station LAN discovery\.$)",
     nullptr,
     "The Core did not find a Tuner Genius at this address on its network. Check the "
     "tuner's address and port.",
     "No matching TGXL discovery announcement for 192.0.2.40:9010. Check the tuner address, "
     "port and station LAN discovery."},
    {R"(^TGXL identity serial mismatch: expected .*, observed .*$)", nullptr,
     "The Tuner Genius at this address is not the one the Core found on its network. Check "
     "the tuner's address and port.",
     "TGXL identity serial mismatch: expected 1234-5678, observed 8765-4321"},
    {R"(^TGXL native info failed with code )", nullptr,
     "The Tuner Genius at this address did not say which unit it is.",
     "TGXL native info failed with code 3"},
    {R"(^TGXL discovery approval timed out for serial )", nullptr,
     "The Core did not see this Tuner Genius on its network in time.",
     "TGXL discovery approval timed out for serial 1234-5678"},

    // The Core's Power Genius XL checks, StationPgxlController.cpp and
    // PgxlConnection.cpp (R-R3-47), each worded around a model, address or
    // serial.
    {R"(^Expected PowerGeniusXL at the connected endpoint; observed .* \(serial .*\)\.$)",
     nullptr,
     "The device at this address is not a Power Genius. Check the amplifier's address and "
     "port.",
     "Expected PowerGeniusXL at the connected endpoint; observed TunerGenius (serial "
     "241288-1)."},
    {R"(^No matching PGXL discovery announcement for .+\. Check the amplifier address, port and station LAN discovery\.$)",
     nullptr,
     "The Core did not find a Power Genius at this address on its network. Check the "
     "amplifier's address and port.",
     "No matching PGXL discovery announcement for 192.0.2.40:9008. Check the amplifier "
     "address, port and station LAN discovery."},
    {R"(^PGXL identity serial mismatch: expected .*, observed .*$)", nullptr,
     "The Power Genius at this address is not the one the Core found on its network. Check "
     "the amplifier's address and port.",
     "PGXL identity serial mismatch: expected 10-200/24-0046, observed 10-200/24-0047"},
    {R"(^PGXL native info failed with code )", nullptr,
     "The Power Genius at this address did not say which unit it is.",
     "PGXL native info failed with code 3"},
    {R"(^PGXL discovery approval timed out for serial )", nullptr,
     "The Core did not see this Power Genius on its network in time.",
     "PGXL discovery approval timed out for serial 10-200/24-0046"},

    // StationClient.cpp: nothing was sent because the link is not up.
    {R"(^The station session is not established, so (.+) was not sent\.$)", nullptr,
     "This app is not connected to the Core right now, so %1 was not sent.",
     "The station session is not established, so the request for a new slice was not sent."},
};

// The general sentence for a reason with no words to show: an empty one,
// or one in the Core's or this app's internal terms.
constexpr char kGeneralSentence[] = "The reason is in the log.";
constexpr char kGeneralShortLine[] = "Refused by the Core";
constexpr char kGeneralShorterLine[] = "Refused";
// What happens next on a pan the Core turned down.
constexpr char kGeneralPanNext[] = "It asks again when you change this pan's view.";

// RemoteMediaController.cpp prefixes the transport's own words with this
// when audio and display cannot start here.
constexpr char kMediaStartPrefix[] = "Station media could not start on this computer";
constexpr char kMediaStartSentence[] = "Audio and display could not start on this computer.";

const Entry* find(const QString& wireReason)
{
    for (const Entry& entry : kEntries) {
        if (wireReason == QLatin1String(entry.wire)) {
            return &entry;
        }
    }
    for (const Entry& entry : kOlderCoreEntries) {
        if (wireReason == QLatin1String(entry.wire)) {
            return &entry;
        }
    }
    return nullptr;
}

struct CompiledPattern {
    QRegularExpression expression;
    const Pattern* pattern;
};

const std::vector<CompiledPattern>& compiledPatterns()
{
    static const std::vector<CompiledPattern> compiled = [] {
        std::vector<CompiledPattern> list;
        for (const Pattern& pattern : kPatterns) {
            list.push_back({QRegularExpression(QString::fromLatin1(pattern.regex)), &pattern});
        }
        return list;
    }();
    return compiled;
}

// The matching pattern and its sentence, or nullptr.
const Pattern* matchPattern(const QString& wireReason, QString* sentence)
{
    for (const CompiledPattern& compiled : compiledPatterns()) {
        const QRegularExpressionMatch match = compiled.expression.match(wireReason);
        if (!match.hasMatch()) {
            continue;
        }
        QString text = QString::fromLatin1(compiled.pattern->sentence);
        if (text.contains(QLatin1String("%1"))) {
            const QString value = match.captured(match.lastCapturedIndex() >= 1 ? 1 : 0);
            // A value in internal terms is left out, with its sentence.
            if (value.trimmed().isEmpty() || !internalTermIn(value).isEmpty()) {
                text = QString::fromLatin1(kGeneralSentence);
            } else {
                text = text.arg(value);
            }
        }
        if (sentence) {
            *sentence = text;
        }
        return compiled.pattern;
    }
    return nullptr;
}

// The raw reason goes to the log whenever the user reads other words, once
// per distinct reason so a panel that refreshes does not repeat it. The
// sites that record a reason log it again each time it happens.
void logRawOnce(const QString& wireReason, const QString& shown)
{
    static QMutex mutex;
    static QSet<QString> logged;
    constexpr qsizetype kMaxRemembered = 512;
    {
        QMutexLocker lock(&mutex);
        if (logged.contains(wireReason)) {
            return;
        }
        if (logged.size() >= kMaxRemembered) {
            logged.clear();
        }
        logged.insert(wireReason);
    }
    qCInfo(lcOperatorReason).noquote()
        << QStringLiteral("Reason shown in user words: raw \"%1\" shown as \"%2\"")
               .arg(wireReason, shown);
}

QString translate(const QString& wireReason)
{
    if (const Entry* entry = find(wireReason)) {
        return QString::fromLatin1(entry->sentence);
    }
    if (wireReason.startsWith(QLatin1String(kMediaStartPrefix))) {
        const QString rest = wireReason.mid(qsizetype(sizeof(kMediaStartPrefix) - 1));
        const QString sentence = QString::fromLatin1(kMediaStartSentence);
        if (rest.startsWith(QLatin1String(": ")) && rest.size() > 2) {
            // The transport's own words follow; shown as sent when plain,
            // otherwise translated like any other reason.
            const QString detail = rest.mid(2);
            const QString shown = translate(detail);
            return shown == detail
                ? sentence + QStringLiteral(" Details: ") + detail
                : sentence + QLatin1Char(' ') + shown;
        }
        if (rest.isEmpty()) {
            return sentence;
        }
    }
    QString sentence;
    if (matchPattern(wireReason, &sentence)) {
        return sentence;
    }
    if (wireReason.trimmed().isEmpty() || !internalTermIn(wireReason).isEmpty()) {
        return QString::fromLatin1(kGeneralSentence);
    }
    // Already in user words: shown as sent.
    return wireReason;
}

} // namespace

const QStringList& internalTerms()
{
    static const QStringList terms{
        // Subsystem and roadmap names.
        QStringLiteral("DSP"), QStringLiteral("WDSP"), QStringLiteral("txPermitted"),
        QStringLiteral("R3"), QStringLiteral("R4"), QStringLiteral("Role"),
        // Internal terms from the R3 user wording plan's Global Constraints.
        QStringLiteral("grant"), QStringLiteral("budget"), QStringLiteral("allocation"),
        QStringLiteral("capabilit"), QStringLiteral("minor"), QStringLiteral("slot"),
        QStringLiteral("epoch"), QStringLiteral("SSRC"), QStringLiteral("endpoint"),
        QStringLiteral("revision"), QStringLiteral("handshake"), QStringLiteral("snapshot"),
        QStringLiteral("codec"), QStringLiteral("payload"), QStringLiteral("peer"),
        QStringLiteral("protocol"), QStringLiteral("session"), QStringLiteral("telemetry"),
        QStringLiteral("RTP"), QStringLiteral("PCM"), QStringLiteral("WebSocket"),
        QStringLiteral("pong"), QStringLiteral("ledger"), QStringLiteral("reservation"),
        QStringLiteral("descriptor"), QStringLiteral("plane"), QStringLiteral("context"),
        QStringLiteral("matcher"),
    };
    return terms;
}

QString internalTermIn(const QString& text)
{
    static const std::vector<QRegularExpression> words = [] {
        std::vector<QRegularExpression> list;
        for (const QString& term : internalTerms()) {
            list.emplace_back(QStringLiteral("\\b") + QRegularExpression::escape(term),
                              QRegularExpression::CaseInsensitiveOption);
        }
        return list;
    }();
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (words[i].match(text).hasMatch()) {
            return internalTerms().at(qsizetype(i));
        }
    }
    return {};
}

QString forDisplay(const QString& wireReason)
{
    const QString shown = translate(wireReason);
    if (shown != wireReason && !wireReason.isEmpty()) {
        logRawOnce(wireReason, shown);
    }
    return shown;
}

QStringList shortFormsForDisplay(const QString& wireReason)
{
    const auto forms = [](const char* shortLine, const char* shorter) {
        if (!shortLine) {
            return QStringList{QString::fromLatin1(kGeneralShortLine),
                               QString::fromLatin1(kGeneralShorterLine)};
        }
        QStringList list{QString::fromLatin1(shortLine)};
        if (shorter) {
            list.append(QString::fromLatin1(shorter));
        }
        return list;
    };
    if (const Entry* entry = find(wireReason)) {
        return forms(entry->shortLine, entry->shorter);
    }
    if (const Pattern* pattern = matchPattern(wireReason, nullptr)) {
        return forms(pattern->shortLine, pattern->shorter);
    }
    return forms(nullptr, nullptr);
}

QString shortForDisplay(const QString& wireReason)
{
    return shortFormsForDisplay(wireReason).constFirst();
}

QString panNextStep(const QString& wireReason)
{
    const char* next = nullptr;
    if (const Entry* entry = find(wireReason)) {
        next = entry->panNext;
    } else if (const Pattern* pattern = matchPattern(wireReason, nullptr)) {
        next = pattern->panNext;
    }
    return QString::fromLatin1(next ? next : kGeneralPanNext);
}

QString lanDiscoveryForDisplay(const QString& lastError)
{
    static const QRegularExpression boundary(QStringLiteral("(?<=\\.) (?=Station LAN )"));
    QStringList shown;
    for (const QString& part : lastError.split(boundary, Qt::SkipEmptyParts)) {
        const QString text = forDisplay(part.trimmed());
        if (!shown.contains(text)) {
            shown.append(text);
        }
    }
    return shown.join(QLatin1Char(' '));
}

QStringList tableKeys()
{
    QStringList keys;
    for (const Entry& entry : kEntries) {
        keys.append(QString::fromLatin1(entry.wire));
    }
    keys.append(QString::fromLatin1(kMediaStartPrefix));
    return keys;
}

QStringList olderCoreKeys()
{
    QStringList keys;
    for (const Entry& entry : kOlderCoreEntries) {
        keys.append(QString::fromLatin1(entry.wire));
    }
    return keys;
}

QStringList knownReasons()
{
    QStringList reasons;
    for (const Entry& entry : kEntries) {
        reasons.append(QString::fromLatin1(entry.wire));
    }
    for (const Entry& entry : kOlderCoreEntries) {
        reasons.append(QString::fromLatin1(entry.wire));
    }
    for (const Pattern& pattern : kPatterns) {
        reasons.append(QString::fromLatin1(pattern.sample));
    }
    reasons.append(QString::fromLatin1(kMediaStartPrefix));
    reasons.append(QString::fromLatin1(kMediaStartPrefix)
                   + QStringLiteral(": media transport factory failed"));
    return reasons;
}

} // namespace NereusSDR::OperatorReasonText
