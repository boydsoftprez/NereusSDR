// =================================================================
// src/gui/meters/DataOutItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-26 - Data out sends on a timer to the endpoint chosen by its
//                 4-char ID: JSON, XML or RAW as Thetis clsDataOut.Update
//                 writes them, naming the slice and radio in place of rx.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "DataOutItem.h"
#include "MeterWidget.h"
#include "ThetisReadingNames.h"
#include "core/mmio/ExternalVariableEngine.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QPainter>
#include <QXmlStreamWriter>

#include <algorithm>

namespace NereusSDR {

namespace {

// A value as Thetis writes it: flt.ToString("0.0#####") / dbl.ToString("0.0#####")
// (MeterManager.cs:17391-17393 [v2.10.3.15]): at least one and at most six
// decimals, invariant decimal point.
QString thetisNumber(double value)
{
    QString text = QString::number(value, 'f', 6);
    while (text.endsWith(QLatin1Char('0')) && text.at(text.size() - 2) != QLatin1Char('.')) {
        text.chop(1);
    }
    return text;
}

struct OutValue {
    QString text;
    bool isString{false};
};

} // namespace

DataOutItem::DataOutItem(QObject* parent)
    : MeterItem(parent)
{
    m_sendTimer.setInterval(m_sendIntervalMs);
    connect(&m_sendTimer, &QTimer::timeout, this, &DataOutItem::sendNow);
    m_sendTimer.start();
}

void DataOutItem::setSendIntervalMs(int ms)
{
    m_sendIntervalMs = std::clamp(ms, kDataOutMinMs, kDataOutMaxMs);
    m_sendTimer.setInterval(m_sendIntervalMs);
}

void DataOutItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    setSendIntervalMs(kDataOutNewIntervalMs);
}

void DataOutItem::paint(QPainter& p, int widgetW, int widgetH)
{
    Q_UNUSED(p); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
}

QString DataOutItem::buildPayload(const QHash<int, double>& readings,
                                  const QHash<QString, QVariant>& published,
                                  QChar sliceLetter, const QString& radioId) const
{
    // Readings by name, in name order. Thetis's readings come from one
    // Dictionary<Reading, object>, so a name appears once; a binding's
    // reading wins over a published value of the same name.
    QMap<QString, OutValue> values;
    for (auto it = readings.cbegin(); it != readings.cend(); ++it) {
        const QString name = thetisReadingName(it.key());
        if (name.isEmpty()) { continue; }
        values.insert(name, OutValue{thetisNumber(it.value()), false});
    }
    for (auto it = published.cbegin(); it != published.cend(); ++it) {
        if (it.key().isEmpty() || values.contains(it.key())) { continue; }
        const int type = it.value().typeId();
        if (type == QMetaType::Double || type == QMetaType::Float) {
            values.insert(it.key(), OutValue{thetisNumber(it.value().toDouble()), false});
        } else {
            values.insert(it.key(), OutValue{it.value().toString(), true});
        }
    }
    const QString slice = sliceLetter.isNull() ? QString() : QString(sliceLetter);

    switch (m_outputFormat) {
    case OutputFormat::Json: {
        // From Thetis MeterManager.cs:17385-17399 [v2.10.3.15]: every value
        // is a string; "rx" becomes "slice" and "radio" is added.
        QJsonObject obj;
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            obj.insert(it.key(), it.value().text);
        }
        obj.insert(QStringLiteral("slice"), slice);
        obj.insert(QStringLiteral("radio"), radioId);
        return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
    case OutputFormat::Xml: {
        // From Thetis MeterManager.cs:17400-17415 [v2.10.3.15]: <data> with
        // one element per reading; <slice> and <radio> in place of <rx>.
        QString out;
        QXmlStreamWriter xml(&out);
        xml.writeStartElement(QStringLiteral("data"));
        xml.writeTextElement(QStringLiteral("slice"), slice);
        xml.writeTextElement(QStringLiteral("radio"), radioId);
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            xml.writeTextElement(it.key(), it.value().text);
        }
        xml.writeEndElement();
        return out;
    }
    case OutputFormat::Raw: {
        // From Thetis MeterManager.cs:17416-17430 [v2.10.3.15]: name:value:
        // pairs, colons removed from string values (strng.Replace(":", "")).
        // The radio id is a string value too, so a MAC loses its colons.
        QString out = QStringLiteral("radio:") + QString(radioId).remove(QLatin1Char(':'))
            + QLatin1Char(':');
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            const QString text = it.value().isString
                ? QString(it.value().text).remove(QLatin1Char(':')) : it.value().text;
            out += QStringLiteral("slice:") + slice + QLatin1Char(':') + it.key()
                + QLatin1Char(':') + text + QLatin1Char(':');
        }
        return out;
    }
    }
    return {};
}

MeterWidget* DataOutItem::hostMeter() const
{
    for (QObject* p = parent(); p; p = p->parent()) {
        if (auto* meter = qobject_cast<MeterWidget*>(p)) { return meter; }
    }
    return nullptr;
}

void DataOutItem::sendNow()
{
    // From Thetis MeterManager.cs:17378-17381 [v2.10.3.15]: nothing is sent
    // without a known endpoint (enqueueOutbound drops an unknown guid and
    // a direction-In endpoint).
    if (m_endpointGuid.isNull()) { return; }
    // An item outside a meter (a record being read or captured) has no
    // readings and sends nothing.
    MeterWidget* meter = hostMeter();
    if (!meter) { return; }
    const QHash<int, double> readings = meter->lastBindingValues();
    QHash<QString, QVariant> published;
    for (MeterItem* item : meter->items()) {
        if (!item || item == this) { continue; }
        const QHash<QString, QVariant> named = item->publishedReadings();
        for (auto it = named.cbegin(); it != named.cend(); ++it) {
            if (!published.contains(it.key())) { published.insert(it.key(), it.value()); }
        }
    }
    ExternalVariableEngine::instance().enqueueOutbound(
        m_endpointGuid, buildPayload(readings, published, m_sliceLetter, m_radioId));
}

QString DataOutItem::serialize() const
{
    // Fields 11 and 12 (send interval, endpoint guid) follow the ten that
    // records written before them carry.
    return QStringLiteral("DATAOUT|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(m_mmioGuid).arg(m_mmioVariable)
        .arg(static_cast<int>(m_outputFormat)).arg(static_cast<int>(m_transportMode))
        + QLatin1Char('|') + QString::number(m_sendIntervalMs)
        + QLatin1Char('|')
        + (m_endpointGuid.isNull() ? QString() : m_endpointGuid.toString(QUuid::WithoutBraces));
}

bool DataOutItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("DATAOUT")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { m_mmioGuid = parts[7]; }
    if (parts.size() > 8) { m_mmioVariable = parts[8]; }
    if (parts.size() > 9) { m_outputFormat = static_cast<OutputFormat>(parts[9].toInt()); }
    if (parts.size() > 10) { m_transportMode = static_cast<TransportMode>(parts[10].toInt()); }
    // A record without these keeps the class's 1000 ms and no endpoint.
    if (parts.size() > 11) {
        bool ok = false;
        const int ms = parts[11].toInt(&ok);
        if (ok) { setSendIntervalMs(ms); }
    }
    if (parts.size() > 12) { m_endpointGuid = QUuid(parts[12]); }
    return true;
}

} // namespace NereusSDR
