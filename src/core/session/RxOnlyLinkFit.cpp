// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RxOnlyLinkFit.cpp  (NereusSDR)
// =================================================================
//
// RX-only antenna values 4 and 5 on the station link. See RxOnlyLinkFit.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/RxOnlyLinkFit.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QStringList>

namespace NereusSDR::RxOnlyLinkFit {

namespace {

// A comma-separated list with each entry above 3 read as 3.
bool narrowList(QJsonValue& value)
{
    if (!value.isString()) {
        return false;
    }
    QStringList parts = value.toString().split(QLatin1Char(','));
    bool changed = false;
    for (QString& part : parts) {
        bool ok = false;
        const int v = part.toInt(&ok);
        if (ok && v > kNarrowMax) {
            part = QString::number(narrowRxOnly(v));
            changed = true;
        }
    }
    if (changed) {
        value = parts.join(QLatin1Char(','));
    }
    return changed;
}

bool fitValue(QJsonValue& value, bool underRxOnly);

bool fitObject(QJsonObject& object, bool underRxOnly)
{
    // A property entry ({"name": ...}) or a write result ({"property": ...})
    // about rxOnlyAntennas: its value is the list, directly or as a typed
    // entry ({"kind": ..., "value": ...}).
    const bool named = object.value(QLatin1String("name")).toString() == QLatin1String(kProperty)
        || object.value(QLatin1String("property")).toString() == QLatin1String(kProperty);
    bool changed = false;
    for (auto it = object.begin(); it != object.end(); ++it) {
        QJsonValue child = it.value();
        const bool listHere = (named || underRxOnly) && it.key() == QLatin1String("value");
        bool fitted = false;
        if (listHere && child.isString()) {
            fitted = narrowList(child);
        } else if (child.isObject() || child.isArray()) {
            fitted = fitValue(child, listHere);
        }
        if (fitted) {
            it.value() = child;
            changed = true;
        }
    }
    return changed;
}

bool fitValue(QJsonValue& value, bool underRxOnly)
{
    if (value.isObject()) {
        QJsonObject object = value.toObject();
        if (!fitObject(object, underRxOnly)) {
            return false;
        }
        value = object;
        return true;
    }
    if (value.isArray()) {
        QJsonArray array = value.toArray();
        bool changed = false;
        for (qsizetype i = 0; i < array.size(); ++i) {
            QJsonValue element = array.at(i);
            if (fitValue(element, false)) {
                array.replace(i, element);
                changed = true;
            }
        }
        if (changed) {
            value = array;
        }
        return changed;
    }
    return false;
}

} // namespace

int narrowRxOnly(int value)
{
    // Thetis names 4 and 5 by the XVTR box (setup.cs:13728-13733
    // [v2.10.3.15]).
    return value > kNarrowMax ? kNarrowMax : value;
}

QByteArray forPeerWithoutPairs(const QByteArray& wire)
{
    if (!wire.contains(kProperty) || (!wire.contains('4') && !wire.contains('5'))) {
        return wire;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(wire, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return wire;
    }
    QJsonValue root = doc.object();
    if (!fitValue(root, false)) {
        return wire;
    }
    return QJsonDocument(root.toObject()).toJson(QJsonDocument::Compact);
}

QString keepPairsFromNarrowList(const QString& current, const QString& written)
{
    const QStringList now = current.split(QLatin1Char(','));
    QStringList kept = written.split(QLatin1Char(','));
    for (qsizetype i = 0; i < kept.size() && i < now.size(); ++i) {
        bool okNow = false;
        bool okWritten = false;
        const int had = now.at(i).toInt(&okNow);
        const int wrote = kept.at(i).toInt(&okWritten);
        if (!okNow || !okWritten) {
            continue;
        }
        if (wrote > kNarrowMax || (had > kNarrowMax && wrote == narrowRxOnly(had))) {
            kept[i] = QString::number(had);
        }
    }
    return kept.join(QLatin1Char(','));
}

} // namespace NereusSDR::RxOnlyLinkFit
