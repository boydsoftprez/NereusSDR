// =================================================================
// src/gui/RemoteTransmitForwarder.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - Created for the desktop remote window's transmit
//                (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

#include "gui/RemoteTransmitForwarder.h"

#include "core/session/RemoteTransmitClient.h"
#include "gui/RemoteMediaController.h"

#include <QPointer>

#include <utility>

namespace NereusSDR {

TciServer::RemoteTransmit remoteTransmitForwarder(RemoteTransmitClient* transmit,
                                                  RemoteMediaController* media)
{
    const QPointer<RemoteTransmitClient> client(transmit);
    const QPointer<RemoteMediaController> uplink(media);
    TciServer::RemoteTransmit forward;
    forward.key = [client](std::function<void(const TciServer::RemoteKeyAnswer&)> answer) {
        if (client.isNull()) {
            TciServer::RemoteKeyAnswer refused;
            refused.reason = QString::fromLatin1(RemoteTransmitClient::kNoLinkReason);
            if (answer) { answer(refused); }
            return;
        }
        client->keyForProgram([answer = std::move(answer)](const RemoteTransmitClient::Answer& r) {
            TciServer::RemoteKeyAnswer converted;
            converted.accepted = r.accepted;
            converted.epoch = r.epoch;
            converted.reason = r.reason;
            if (answer) { answer(converted); }
        });
    };
    forward.unkey = [client](quint32 epoch) {
        if (client) { client->unkeyForProgram(epoch); }
    };
    forward.audio = [uplink](const float* samples, int frames, int channels, int sampleRate) {
        if (uplink) { uplink->pushProgramAudio(samples, frames, channels, sampleRate); }
    };
    return forward;
}

} // namespace NereusSDR
