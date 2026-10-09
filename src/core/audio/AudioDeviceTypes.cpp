// =================================================================
// src/core/audio/AudioDeviceTypes.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Keys, labels and the pair model for
// the audio engines (R-AUD-07); no upstream logic.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03, R-AUD-07). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioDeviceTypes.h"

#include <QChar>

#include <array>

namespace NereusSDR {

namespace {

struct EngineEntry {
    AudioEngineKind kind;
    const char* key;
    const char* label;
    AudioBackendId backend;
};

constexpr std::array<EngineEntry, 8> kEngines{{
    {AudioEngineKind::PortAudio, "PortAudio", "Older drivers", AudioBackendId::PortAudio},
    {AudioEngineKind::CoreAudio, "CoreAudio", "Core Audio", AudioBackendId::CoreAudio},
    {AudioEngineKind::WindowsShared, "WindowsShared", "Windows audio, shared", AudioBackendId::Wasapi},
    {AudioEngineKind::WindowsExclusive, "WindowsExclusive", "Windows audio, exclusive", AudioBackendId::Wasapi},
    {AudioEngineKind::Asio, "ASIO", "ASIO", AudioBackendId::Asio},
    {AudioEngineKind::PipeWire, "PipeWire", "PipeWire", AudioBackendId::PipeWire},
    {AudioEngineKind::PulseAudio, "PulseAudio", "PulseAudio", AudioBackendId::PulseAudio},
    {AudioEngineKind::AlsaDirect, "AlsaDirect", "ALSA, direct", AudioBackendId::AlsaDirect},
}};

const EngineEntry& entryFor(AudioEngineKind kind)
{
    for (const EngineEntry& e : kEngines) {
        if (e.kind == kind) {
            return e;
        }
    }
    return kEngines.front();
}

} // namespace

QString audioEngineKey(AudioEngineKind kind)
{
    return QString::fromLatin1(entryFor(kind).key);
}

std::optional<AudioEngineKind> audioEngineFromKey(const QString& key)
{
    for (const EngineEntry& e : kEngines) {
        if (key == QLatin1String(e.key)) {
            return e.kind;
        }
    }
    return std::nullopt;
}

QString audioEngineLabel(AudioEngineKind kind)
{
    return QString::fromLatin1(entryFor(kind).label);
}

AudioBackendId audioBackendFor(AudioEngineKind kind)
{
    return entryFor(kind).backend;
}

QList<AudioChannelPair> audioChannelPairs(int channelCount)
{
    QList<AudioChannelPair> pairs;
    if (channelCount < 1) {
        return pairs;
    }
    if (channelCount == 1) {
        pairs.append(AudioChannelPair{1, 1});
        return pairs;
    }
    int first = 1;
    for (; first + 1 <= channelCount; first += 2) {
        pairs.append(AudioChannelPair{first, 2});
    }
    if (first == channelCount) {
        pairs.append(AudioChannelPair{first, 1});
    }
    return pairs;
}

QString audioPairLabel(AudioDeviceDirection direction, const AudioChannelPair& pair)
{
    const bool output = direction == AudioDeviceDirection::Output;
    if (pair.channelCount <= 1) {
        return (output ? QStringLiteral("Output %1") : QStringLiteral("Input %1"))
            .arg(pair.firstChannel);
    }
    return (output ? QStringLiteral("Outputs %1-%2") : QStringLiteral("Inputs %1-%2"))
        .arg(pair.firstChannel)
        .arg(pair.firstChannel + pair.channelCount - 1);
}

QString audioDeviceEntryLabel(const AudioDeviceInfo& info, const AudioChannelPair& pair)
{
    if (info.channelCount <= 2) {
        return info.name;
    }
    return info.name + QLatin1Char(' ') + QChar(0x00B7) + QLatin1Char(' ')
        + audioPairLabel(info.direction, pair);
}

QString micChannelKey(MicChannelPick pick)
{
    switch (pick) {
    case MicChannelPick::Left:
        return QStringLiteral("Left");
    case MicChannelPick::Right:
        return QStringLiteral("Right");
    case MicChannelPick::Both:
        return QStringLiteral("Both");
    }
    return QStringLiteral("Left");
}

std::optional<MicChannelPick> micChannelFromKey(const QString& key)
{
    if (key == QLatin1String("Left")) {
        return MicChannelPick::Left;
    }
    if (key == QLatin1String("Right")) {
        return MicChannelPick::Right;
    }
    if (key == QLatin1String("Both")) {
        return MicChannelPick::Both;
    }
    return std::nullopt;
}

} // namespace NereusSDR
