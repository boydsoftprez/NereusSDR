// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/BandStackCommand.cpp  (NereusSDR)
// =================================================================
//
// See BandStackCommand.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "models/bandstack/BandStackCommand.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>

namespace NereusSDR {

namespace {

QString directionName(BandStackDirection direction)
{
    return direction == BandStackDirection::Next ? QStringLiteral("next")
                                                 : QStringLiteral("previous");
}

QString returnModeName(BandStackReturnMode mode)
{
    switch (mode) {
    case BandStackReturnMode::Current:
        return QStringLiteral("current");
    case BandStackReturnMode::LastUsed:
        return QStringLiteral("lastUsed");
    case BandStackReturnMode::Specific:
        return QStringLiteral("specific");
    }
    return QStringLiteral("current");
}

void insertView(QJsonObject& arguments, const BandStackView& view)
{
    if (view.centerHz.has_value()) {
        arguments.insert(QStringLiteral("viewCenterHz"), *view.centerHz);
    }
    if (view.spanHz.has_value()) {
        arguments.insert(QStringLiteral("viewSpanHz"), *view.spanHz);
    }
}

} // namespace

QList<MirrorUpdate> BandStackCommand::wireArguments() const
{
    QList<MirrorUpdate> wire;
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        const QByteArray name = it.key().toUtf8();
        const QJsonValue value = it.value();
        if (value.isString()) {
            wire.append(MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value.toString())});
        } else if (value.isBool()) {
            wire.append(MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(value.toBool())});
        } else {
            wire.append(MirrorUpdate{0, name, MirrorWireKind::Int64,
                                     QVariant(static_cast<qlonglong>(value.toInteger()))});
        }
    }
    return wire;
}

BandStackCommand BandStackCommand::press(int sliceId, Band band, BandStackDirection direction,
                                         const BandStackView& view)
{
    QJsonObject arguments{{QStringLiteral("sliceId"), sliceId},
                          {QStringLiteral("band"), bandKeyName(band)},
                          {QStringLiteral("direction"), directionName(direction)}};
    insertView(arguments, view);
    return {QStringLiteral("bandStack.press"), arguments};
}

BandStackCommand BandStackCommand::step(int sliceId, BandStackDirection direction,
                                        const BandStackView& view)
{
    QJsonObject arguments{{QStringLiteral("sliceId"), sliceId},
                          {QStringLiteral("direction"), directionName(direction)}};
    insertView(arguments, view);
    return {QStringLiteral("bandStack.step"), arguments};
}

BandStackCommand BandStackCommand::goTo(int sliceId, const QString& entryId,
                                        const BandStackView& view)
{
    QJsonObject arguments{{QStringLiteral("sliceId"), sliceId},
                          {QStringLiteral("entryId"), entryId}};
    insertView(arguments, view);
    return {QStringLiteral("bandStack.goTo"), arguments};
}

BandStackCommand BandStackCommand::add(int sliceId, const BandStackView& view)
{
    QJsonObject arguments{{QStringLiteral("sliceId"), sliceId}};
    insertView(arguments, view);
    return {QStringLiteral("bandStack.add"), arguments};
}

BandStackCommand BandStackCommand::saveHere(int sliceId, const QString& entryId,
                                            const BandStackView& view)
{
    QJsonObject arguments{{QStringLiteral("sliceId"), sliceId},
                          {QStringLiteral("entryId"), entryId}};
    insertView(arguments, view);
    return {QStringLiteral("bandStack.saveHere"), arguments};
}

BandStackCommand BandStackCommand::edit(const QString& entryId, const std::optional<QString>& name,
                                        std::optional<bool> locked)
{
    QJsonObject arguments{{QStringLiteral("entryId"), entryId}};
    if (name.has_value()) {
        arguments.insert(QStringLiteral("name"), *name);
    }
    if (locked.has_value()) {
        arguments.insert(QStringLiteral("locked"), *locked);
    }
    return {QStringLiteral("bandStack.edit"), arguments};
}

BandStackCommand BandStackCommand::remove(const QString& entryId)
{
    return {QStringLiteral("bandStack.delete"), QJsonObject{{QStringLiteral("entryId"), entryId}}};
}

BandStackCommand BandStackCommand::reorder(Band band, const QStringList& ids)
{
    // The link has no list kind: the verb takes the list as JSON text.
    const QString text = QString::fromUtf8(
        QJsonDocument(QJsonArray::fromStringList(ids)).toJson(QJsonDocument::Compact));
    return {QStringLiteral("bandStack.reorder"),
            QJsonObject{{QStringLiteral("band"), bandKeyName(band)},
                        {QStringLiteral("ids"), text}}};
}

BandStackCommand BandStackCommand::setReturn(Band band, BandStackReturnMode mode,
                                             const QString& entryId)
{
    QJsonObject arguments{{QStringLiteral("band"), bandKeyName(band)},
                          {QStringLiteral("mode"), returnModeName(mode)}};
    if (!entryId.isEmpty()) {
        arguments.insert(QStringLiteral("entryId"), entryId);
    }
    return {QStringLiteral("bandStack.setReturn"), arguments};
}

BandStackCommand BandStackCommand::clear(std::optional<Band> band)
{
    QJsonObject arguments;
    if (band.has_value()) {
        arguments.insert(QStringLiteral("band"), bandKeyName(*band));
    }
    return {QStringLiteral("bandStack.clear"), arguments};
}

BandStackCommand BandStackCommand::reseed()
{
    return {QStringLiteral("bandStack.reseed"), QJsonObject{}};
}

BandStackCommand BandStackCommand::options(std::optional<bool> dupes,
                                           std::optional<int> expiryMinutes,
                                           std::optional<int> dwellSeconds)
{
    QJsonObject arguments;
    if (dupes.has_value()) {
        arguments.insert(QStringLiteral("dupeProtection"), *dupes);
    }
    if (expiryMinutes.has_value()) {
        arguments.insert(QStringLiteral("expiryMinutes"), *expiryMinutes);
    }
    if (dwellSeconds.has_value()) {
        arguments.insert(QStringLiteral("dwellSeconds"), *dwellSeconds);
    }
    return {QStringLiteral("bandStack.options"), arguments};
}

BandStackCommand BandStackCommand::restoreExpired(const QString& batchId)
{
    return {QStringLiteral("bandStack.restoreExpired"),
            QJsonObject{{QStringLiteral("batchId"), batchId}}};
}

} // namespace NereusSDR
