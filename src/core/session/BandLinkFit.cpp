// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/BandLinkFit.cpp  (NereusSDR)
// =================================================================
//
// 2 m on the station link. See BandLinkFit.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), 2 m as its own band (R-IOS-26, R-R3-49), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/BandLinkFit.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QString>
#include <QStringList>

namespace NereusSDR::BandLinkFit {

namespace {

enum class Direction { ToPeer, ToStation };

// Properties whose value is one band number.
bool isBandNumberProperty(const QString& name)
{
    return name == QLatin1String("band") || name == QLatin1String("rxFilter0Band")
        || name == QLatin1String("rxFilter1Band") || name == QLatin1String("bandOutputsBand");
}

// AlexAntennaFacade's per-band antenna lists: one comma-separated entry per
// band, 2 m's last.
bool isAntennaListProperty(const QString& name)
{
    return name == QLatin1String("rxAntennas") || name == QLatin1String("rxOnlyAntennas")
        || name == QLatin1String("txAntennas");
}

// TransmitModel's per-band watts: a JSON object keyed by bandKeyName.
bool isBandWattsProperty(const QString& name)
{
    return name == QLatin1String("powerByBandJson") || name == QLatin1String("tunePowerByBandJson");
}

bool is2mNumber(const QJsonValue& v)
{
    return v.isDouble() && v.toDouble() == static_cast<double>(k2mNumber);
}

QString compact(const QJsonValue& v)
{
    const QJsonDocument doc = v.isArray() ? QJsonDocument(v.toArray()) : QJsonDocument(v.toObject());
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

bool fitValue(QJsonValue& value, Direction direction);

// A string value that holds JSON (the catalogue, connectedDevices'
// listJson): fitted inside, and written back only when something changed.
bool fitJsonText(QJsonValue& value)
{
    const QString text = value.toString();
    if (text.isEmpty() || (text.front() != QLatin1Char('{') && text.front() != QLatin1Char('['))
        || !text.contains(QLatin1String("27"))) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    QJsonValue inner = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    if (!fitValue(inner, Direction::ToPeer)) {
        return false;
    }
    value = compact(inner);
    return true;
}

bool fitAntennaList(QJsonValue& value)
{
    if (!value.isString()) {
        return false;
    }
    QStringList parts = value.toString().split(QLatin1Char(','));
    if (parts.size() != kListEntriesWithout2m + 1) {
        return false;
    }
    parts.removeLast();
    value = parts.join(QLatin1Char(','));
    return true;
}

bool fitBandWatts(QJsonValue& value)
{
    if (!value.isString()) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(value.toString().toUtf8());
    if (!doc.isObject() || !doc.object().contains(QLatin1String("2m"))) {
        return false;
    }
    QJsonObject object = doc.object();
    object.remove(QLatin1String("2m"));
    value = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return true;
}

// One property entry ({"name": ..., "value": ...}).
bool fitPropertyEntry(QJsonObject& entry, Direction direction)
{
    const QString name = entry.value(QLatin1String("name")).toString();
    QJsonValue value = entry.value(QLatin1String("value"));
    bool changed = false;
    if (isAntennaListProperty(name)) {
        changed = fitAntennaList(value);
    } else if (isBandWattsProperty(name)) {
        changed = fitBandWatts(value);
    } else if (direction == Direction::ToPeer && isBandNumberProperty(name)) {
        if (is2mNumber(value)) {
            value = kGenNumber;
            changed = true;
        }
    } else if (direction == Direction::ToPeer && value.isString()) {
        changed = fitJsonText(value);
    }
    if (changed) {
        entry.insert(QLatin1String("value"), value);
    }
    return changed;
}

bool fitObject(QJsonObject& object, Direction direction)
{
    bool changed = false;
    if (object.contains(QLatin1String("name")) && object.contains(QLatin1String("value"))
        && object.value(QLatin1String("name")).isString()) {
        changed = fitPropertyEntry(object, direction);
    }
    if (direction == Direction::ToStation) {
        // A window's own messages carry the lists and maps only as
        // property entries (in property.write and settings); nothing else
        // in them names 2 m.
        for (auto it = object.begin(); it != object.end(); ++it) {
            QJsonValue child = it.value();
            if ((child.isArray() || child.isObject()) && fitValue(child, direction)) {
                it.value() = child;
                changed = true;
            }
        }
        return changed;
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString key = it.key();
        QJsonValue child = it.value();
        if (key == QLatin1String("band") && is2mNumber(child)) {
            it.value() = kGenNumber;
            changed = true;
            continue;
        }
        if (key == QLatin1String("bands") && child.isArray()) {
            // The catalogue's band grid: 2 m's button left out.
            QJsonArray kept;
            bool dropped = false;
            for (const QJsonValue& entry : child.toArray()) {
                if (entry.isObject() && is2mNumber(entry.toObject().value(QLatin1String("id")))) {
                    dropped = true;
                    continue;
                }
                kept.append(entry);
            }
            QJsonValue fitted = kept;
            const bool innerChanged = fitValue(fitted, direction);
            if (dropped || innerChanged) {
                it.value() = fitted;
                changed = true;
            }
            continue;
        }
        if ((child.isArray() || child.isObject()) && fitValue(child, direction)) {
            it.value() = child;
            changed = true;
        }
    }
    return changed;
}

bool fitValue(QJsonValue& value, Direction direction)
{
    if (value.isObject()) {
        QJsonObject object = value.toObject();
        if (!fitObject(object, direction)) {
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
            // A labelled per-band row about 2 m (Setup's antenna rows) is
            // left out: shown as GEN it would be a second GEN row. A
            // slice's or device's band (no label) reads GEN instead.
            if (direction == Direction::ToPeer && element.isObject()
                && element.toObject().contains(QLatin1String("label"))
                && is2mNumber(element.toObject().value(QLatin1String("band")))) {
                array.removeAt(i);
                --i;
                changed = true;
                continue;
            }
            if (fitValue(element, direction)) {
                array.replace(i, element);
                changed = true;
            }
        }
        if (!changed) {
            return false;
        }
        value = array;
        return true;
    }
    return false;
}

QByteArray fitWire(const QByteArray& wire, Direction direction)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(wire, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return wire;
    }
    QJsonValue root = doc.object();
    if (!fitValue(root, direction)) {
        return wire;
    }
    return QJsonDocument(root.toObject()).toJson(QJsonDocument::Compact);
}

} // namespace

QByteArray forPeerWithout2m(const QByteArray& wire)
{
    // Every 2 m trace carries the number 27, an antenna list's fifteenth
    // entry or the "2m" key; a message with neither is sent as it is.
    if (!wire.contains("27") && !wire.contains("2m") && !wire.contains("Antennas")) {
        return wire;
    }
    return fitWire(wire, Direction::ToPeer);
}

QByteArray forStationWithout2m(const QByteArray& wire)
{
    if (!wire.contains("Antennas") && !wire.contains("ByBandJson")) {
        return wire;
    }
    return fitWire(wire, Direction::ToStation);
}

} // namespace NereusSDR::BandLinkFit
