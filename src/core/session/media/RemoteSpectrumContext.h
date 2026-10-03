// =================================================================
// src/core/session/media/RemoteSpectrumContext.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The one wire codec Core and GUI share
// for the spectrum display context, in the minor-8 shape and the minor-9
// shape that also reports what Core granted the endpoint; it holds no
// session identity or subscription policy.
//
// Modification history (NereusSDR):
//   2026-09-26 : Parity Task 28 (R-R3-49, A11): `transmit`. J.J. Boyd
//                 (KG4VCF), AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

#pragma once

#include "core/session/media/SpectrumEndpoint.h"
#include "core/session/media/WidebandDisplayContext.h"
#include "core/spectrum/FftEnginePool.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

/// none, largest-size, shared or source-bins.
QString spectrumLimitReasonToWire(SpectrumLimitReason reason);
/// One of the four wire strings exactly; nullopt for any other string and
/// for any value that is not a string.
std::optional<SpectrumLimitReason> spectrumLimitReasonFromWire(const QJsonValue& value);

/// What a minor-9 context reports of Core's SpectrumGrant (R-R3-01, R-R3-08).
/// The requested FFT size stays with Core; the GUI knows what it asked for.
struct SpectrumContextGrant {
    int grantedFftSize {0};
    FftTier grantedTier {FftTier::Wide};
    int requestedPixels {0};
    int grantedPixels {0};
    SpectrumLimitReason limit {SpectrumLimitReason::None};

    bool operator==(const SpectrumContextGrant&) const = default;
};

/// The reported part of a Core grant.
SpectrumContextGrant spectrumContextGrant(const SpectrumGrant& grant);

struct SpectrumContextMessage {
    QString connectionId;
    quint32 endpointId {0};
    quint32 revision {0};
    quint32 contextGeneration {0};
    int sourceStream {0};
    double sourceCentreHz {0.0};
    double sampleRateHz {0.0};
    double centreHz {0.0};
    double spanHz {0.0};
    double wideCentreHz {0.0};
    double wideSpanHz {0.0};
    int traceSamples {0};
    int waterfallSamples {0};
    int wideSamples {0};
    double minDbm {0.0};
    double maxDbm {0.0};
    int fps {0};
    int framesPerLine {0};
    /// Written when the subscription negotiated the extended view (minor 6).
    std::optional<WidebandDisplayContext> wideband;
    /// Set only when the grant report was negotiated (minor 9).
    std::optional<SpectrumContextGrant> grant;
    /// Parity Task 28: set only for a peer that declared txDisplayVersion in
    /// its media start: true while the endpoint shows the transmit display.
    std::optional<bool> transmit;
};

// grantNegotiated=false: exactly today's 19 keys (op, connectionId,
// endpointId, revision, contextGeneration, sourceStream, sourceCentreHz,
// sampleRateHz, centreHz, spanHz, wideCentreHz, wideSpanHz, traceSamples,
// waterfallSamples, wideSamples, minDbm, maxDbm, fps, framesPerLine), plus
// "wideband" when message.wideband is set, with today's JSON number types.
// The grant is not written.
// grantNegotiated=true: those keys plus grantedFftSize, grantedTier ("wide"
// or "fine"), requestedPixels, grantedPixels and limit. A message with no
// grant is written in the minor-8 shape, which a minor-9 decoder refuses:
// the caller always supplies the grant Core recorded.
QJsonObject encodeRemoteSpectrumContext(const SpectrumContextMessage& message,
                                        bool grantNegotiated);

// Shape and field validation only (session identity, revision, generation
// order and wideband geometry stay with the caller). Accepts exactly the
// shape selected by grantNegotiated, with or without "wideband"; nullopt
// otherwise. The field ranges are the ones the GUI's context parser has
// always applied.
// transmitNegotiated=true (parity Task 28, a GUI that declared
// txDisplayVersion): the grant shape plus exactly one more key, `transmit`, a
// boolean; refused without the grant shape.
std::optional<SpectrumContextMessage> decodeRemoteSpectrumContext(const QJsonObject& payload,
                                                                  bool grantNegotiated,
                                                                  bool transmitNegotiated = false);

} // namespace NereusSDR
