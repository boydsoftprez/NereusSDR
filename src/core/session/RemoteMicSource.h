#pragma once
// no-port-check: NereusSDR-original. Authenticated session microphone choice.
// 2026-10-02: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QString>
#include <optional>

namespace NereusSDR {
enum class RemoteMicSource { ClientAudio, RadioMic };
inline QString remoteMicSourceName(RemoteMicSource source)
{
    return source == RemoteMicSource::RadioMic ? QStringLiteral("RadioMic")
                                              : QStringLiteral("ClientAudio");
}
inline std::optional<RemoteMicSource> remoteMicSourceFromName(const QString& name)
{
    if (name == QLatin1String("ClientAudio")) { return RemoteMicSource::ClientAudio; }
    if (name == QLatin1String("RadioMic")) { return RemoteMicSource::RadioMic; }
    return std::nullopt;
}
inline QString remoteRadioVoxReason()
{ return QStringLiteral("VOX from the radio microphone is not available from this window."); }
inline QString remoteRadioProgramReason()
{ return QStringLiteral("Choose PC/VAX input to transmit program audio."); }
inline QString remoteMicLegacyReason()
{ return QStringLiteral("This Core does not support selecting the radio microphone from this window."); }
} // namespace NereusSDR
