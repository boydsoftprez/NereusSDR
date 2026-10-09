// =================================================================
// src/core/audio/CoreSpeakerJson.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CoreSpeakerJson.h; no upstream
// logic.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 21 (R-AUD-25, R-AUD-28, R-AUD-30).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CoreSpeakerJson.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <cmath>
#include <limits>

namespace NereusSDR {

namespace {

QString compact(const QJsonObject& o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

std::optional<QJsonObject> objectWithKeys(const QJsonObject& o, QStringList keys)
{
    QStringList have = o.keys();
    have.sort();
    keys.sort();
    if (have != keys) {
        return std::nullopt;
    }
    return o;
}

std::optional<QJsonObject> parseObject(const QString& json, const QStringList& keys)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return objectWithKeys(doc.object(), keys);
}

std::optional<int> wholeNumber(const QJsonValue& v)
{
    if (!v.isDouble()) {
        return std::nullopt;
    }
    const double d = v.toDouble();
    if (!std::isfinite(d) || std::floor(d) != d
        || d < static_cast<double>(std::numeric_limits<int>::min())
        || d > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(d);
}

QString stateWord(CoreSpeakerStateKind kind)
{
    switch (kind) {
    case CoreSpeakerStateKind::Playing: return QStringLiteral("playing");
    case CoreSpeakerStateKind::NotConnected: return QStringLiteral("notConnected");
    case CoreSpeakerStateKind::InUse: return QStringLiteral("inUse");
    case CoreSpeakerStateKind::NoCard: return QStringLiteral("noCard");
    case CoreSpeakerStateKind::WaitingForPick: return QStringLiteral("waitingForPick");
    }
    return QStringLiteral("noCard");
}

std::optional<CoreSpeakerStateKind> stateFromWord(const QString& word)
{
    if (word == QLatin1String("playing")) { return CoreSpeakerStateKind::Playing; }
    if (word == QLatin1String("notConnected")) { return CoreSpeakerStateKind::NotConnected; }
    if (word == QLatin1String("inUse")) { return CoreSpeakerStateKind::InUse; }
    if (word == QLatin1String("noCard")) { return CoreSpeakerStateKind::NoCard; }
    if (word == QLatin1String("waitingForPick")) { return CoreSpeakerStateKind::WaitingForPick; }
    return std::nullopt;
}

QString cardStateWord(AudioDeviceState state)
{
    switch (state) {
    case AudioDeviceState::Present: return QStringLiteral("present");
    case AudioDeviceState::NotConnected: return QStringLiteral("notConnected");
    case AudioDeviceState::InUse: return QStringLiteral("inUse");
    }
    return QStringLiteral("present");
}

std::optional<AudioDeviceState> cardStateFromWord(const QString& word)
{
    if (word == QLatin1String("present")) { return AudioDeviceState::Present; }
    if (word == QLatin1String("notConnected")) { return AudioDeviceState::NotConnected; }
    if (word == QLatin1String("inUse")) { return AudioDeviceState::InUse; }
    return std::nullopt;
}

} // namespace

QString coreSpeakerStateToJson(const CoreSpeakerState& state)
{
    QJsonObject o;
    o.insert(QStringLiteral("state"), stateWord(state.kind));
    o.insert(QStringLiteral("playing"), state.playingName);
    o.insert(QStringLiteral("chosen"), state.chosenName);
    o.insert(QStringLiteral("desktop"), state.desktop);
    return compact(o);
}

std::optional<CoreSpeakerState> coreSpeakerStateFromJson(const QString& json)
{
    const std::optional<QJsonObject> o = parseObject(
        json, {QStringLiteral("state"), QStringLiteral("playing"), QStringLiteral("chosen"),
               QStringLiteral("desktop")});
    if (!o) {
        return std::nullopt;
    }
    const QJsonValue state = o->value(QStringLiteral("state"));
    const QJsonValue playing = o->value(QStringLiteral("playing"));
    const QJsonValue chosen = o->value(QStringLiteral("chosen"));
    const QJsonValue desktop = o->value(QStringLiteral("desktop"));
    if (!state.isString() || !playing.isString() || !chosen.isString() || !desktop.isBool()) {
        return std::nullopt;
    }
    const std::optional<CoreSpeakerStateKind> kind = stateFromWord(state.toString());
    if (!kind) {
        return std::nullopt;
    }
    CoreSpeakerState out;
    out.kind = *kind;
    out.playingName = playing.toString();
    out.chosenName = chosen.toString();
    out.desktop = desktop.toBool();
    return out;
}

QString coreSpeakerDevicesToJson(const QList<CoreSpeakerCard>& cards)
{
    QJsonArray a;
    for (const CoreSpeakerCard& card : cards) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), card.id);
        o.insert(QStringLiteral("name"), card.name);
        o.insert(QStringLiteral("state"), cardStateWord(card.state));
        a.append(o);
    }
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

std::optional<QList<CoreSpeakerCard>> coreSpeakerDevicesFromJson(const QString& json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return std::nullopt;
    }
    QList<CoreSpeakerCard> cards;
    for (const QJsonValue& v : doc.array()) {
        if (!v.isObject()) {
            return std::nullopt;
        }
        const std::optional<QJsonObject> o = objectWithKeys(
            v.toObject(), {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("state")});
        if (!o) {
            return std::nullopt;
        }
        const QJsonValue id = o->value(QStringLiteral("id"));
        const QJsonValue name = o->value(QStringLiteral("name"));
        const QJsonValue state = o->value(QStringLiteral("state"));
        if (!id.isString() || !name.isString() || !state.isString()) {
            return std::nullopt;
        }
        const std::optional<AudioDeviceState> cardState = cardStateFromWord(state.toString());
        if (!cardState) {
            return std::nullopt;
        }
        cards.append(CoreSpeakerCard{id.toString(), name.toString(), *cardState});
    }
    return cards;
}

QString coreSpeakerDeviceToJson(const QString& id, const QString& name)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("name"), name);
    return compact(o);
}

std::optional<QPair<QString, QString>> coreSpeakerDeviceFromJson(const QString& json)
{
    const std::optional<QJsonObject> o =
        parseObject(json, {QStringLiteral("id"), QStringLiteral("name")});
    if (!o) {
        return std::nullopt;
    }
    const QJsonValue id = o->value(QStringLiteral("id"));
    const QJsonValue name = o->value(QStringLiteral("name"));
    if (!id.isString() || !name.isString()) {
        return std::nullopt;
    }
    return qMakePair(id.toString(), name.toString());
}

QString coreSpeakerDetailsToJson(const CoreSpeakerDetails& details)
{
    QJsonObject o;
    o.insert(QStringLiteral("bufferFrames"), details.bufferFrames);
    o.insert(QStringLiteral("delayMs"), details.delayMs);
    o.insert(QStringLiteral("sampleRate"), details.sampleRate);
    o.insert(QStringLiteral("negotiated"), details.negotiated);
    o.insert(QStringLiteral("delayNowMs"), details.delayNowMs);
    return compact(o);
}

std::optional<CoreSpeakerDetails> coreSpeakerDetailsFromJson(const QString& json)
{
    const std::optional<QJsonObject> o = parseObject(
        json, {QStringLiteral("bufferFrames"), QStringLiteral("delayMs"),
               QStringLiteral("sampleRate"), QStringLiteral("negotiated"),
               QStringLiteral("delayNowMs")});
    if (!o) {
        return std::nullopt;
    }
    const std::optional<int> buffer = wholeNumber(o->value(QStringLiteral("bufferFrames")));
    const std::optional<int> delay = wholeNumber(o->value(QStringLiteral("delayMs")));
    const std::optional<int> rate = wholeNumber(o->value(QStringLiteral("sampleRate")));
    const QJsonValue negotiated = o->value(QStringLiteral("negotiated"));
    const QJsonValue delayNow = o->value(QStringLiteral("delayNowMs"));
    if (!buffer || !delay || !rate || !negotiated.isString() || !delayNow.isDouble()
        || !std::isfinite(delayNow.toDouble())) {
        return std::nullopt;
    }
    CoreSpeakerDetails out;
    out.bufferFrames = *buffer;
    out.delayMs = *delay;
    out.sampleRate = *rate;
    out.negotiated = negotiated.toString();
    out.delayNowMs = delayNow.toDouble();
    return out;
}

CoreSpeakerState coreSpeakerStateFor(const AudioRoleStatus& status, bool desktop)
{
    CoreSpeakerState out;
    out.desktop = desktop;
    out.chosenName = status.chosenName;
    switch (status.state) {
    case AudioRoleState::Playing:
        out.kind = CoreSpeakerStateKind::Playing;
        out.playingName = status.playingName;
        break;
    case AudioRoleState::PlayingOnDefault:
        out.playingName = status.playingName;
        if (status.reason == AudioRoleReason::InUse) {
            out.kind = CoreSpeakerStateKind::InUse;
        } else if (status.reason == AudioRoleReason::None) {
            out.kind = CoreSpeakerStateKind::Playing;
        } else {
            out.kind = CoreSpeakerStateKind::NotConnected;
        }
        break;
    case AudioRoleState::Silent:
        if (status.reason == AudioRoleReason::NotConnected) {
            out.kind = CoreSpeakerStateKind::NotConnected;
        } else if (status.reason == AudioRoleReason::InUse) {
            out.kind = CoreSpeakerStateKind::InUse;
        } else {
            out.kind = CoreSpeakerStateKind::NoCard;
        }
        break;
    case AudioRoleState::WaitingForPick:
        out.kind = CoreSpeakerStateKind::WaitingForPick;
        break;
    case AudioRoleState::Off:
        out.kind = CoreSpeakerStateKind::NoCard;
        break;
    }
    return out;
}

} // namespace NereusSDR
