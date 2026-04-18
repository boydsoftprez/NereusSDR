// =================================================================
// src/core/mmio/MmioEndpoint.cpp  (NereusSDR)
// =================================================================
//
// Independently implemented from MmioEndpoint.h interface.
// The .h declares the Thetis-derived clsMMIO payload-format enum
// (cited per MeterManager.cs in PROVENANCE). This .cpp is pure Qt
// QObject lifecycle scaffolding around that enum — no MeterManager
// logic, no Samphire-authored algorithm. Original NereusSDR work
// licensed under GPLv3.
// =================================================================

#include "MmioEndpoint.h"

#include "../LogCategories.h"

namespace NereusSDR {

MmioEndpoint::MmioEndpoint(QObject* parent)
    : QObject(parent)
{
}

MmioEndpoint::~MmioEndpoint() = default;

QVariant MmioEndpoint::value(const QString& key) const
{
    QReadLocker locker(&m_lock);
    return m_values.value(key);
}

QVariant MmioEndpoint::valueForName(const QString& variableName) const
{
    const QString key = m_guid.toString(QUuid::WithoutBraces)
                       + QLatin1Char('.') + variableName;
    return value(key);
}

QDateTime MmioEndpoint::lastUpdate(const QString& variableName) const
{
    const QString key = m_guid.toString(QUuid::WithoutBraces)
                       + QLatin1Char('.') + variableName;
    QReadLocker locker(&m_lock);
    return m_lastUpdate.value(key);
}

QStringList MmioEndpoint::variableNames() const
{
    QReadLocker locker(&m_lock);
    QStringList names;
    names.reserve(m_values.size());
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        names.append(stripGuidPrefix(it.key()));
    }
    names.sort();
    return names;
}

void MmioEndpoint::mergeBatch(const QHash<QString, QVariant>& batch)
{
    bool discovered = false;
    const QDateTime now = QDateTime::currentDateTime();

    {
        QWriteLocker locker(&m_lock);
        for (auto it = batch.constBegin(); it != batch.constEnd(); ++it) {
            if (!m_values.contains(it.key())) {
                discovered = true;
            }
            m_values.insert(it.key(), it.value());
            m_lastUpdate.insert(it.key(), now);
        }
    }

    if (discovered) {
        emit variablesDiscovered();
    }
}

QString MmioEndpoint::stripGuidPrefix(const QString& key) const
{
    const QString prefix = m_guid.toString(QUuid::WithoutBraces)
                         + QLatin1Char('.');
    if (key.startsWith(prefix)) {
        return key.mid(prefix.size());
    }
    return key;
}

} // namespace NereusSDR
