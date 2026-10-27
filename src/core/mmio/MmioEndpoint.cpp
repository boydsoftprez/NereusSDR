// =================================================================
// src/core/mmio/MmioEndpoint.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Independently implemented as Qt QObject lifecycle
//                 scaffolding and the variable cache. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code: clsMMIO 4-character ID, EnqueueOutbound /
//                 DequeueOutbound, custom terminator parsing and the
//                 outbound terminator.
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

#include "MmioEndpoint.h"

#include "FourChar.h"
#include "../LogCategories.h"

#include <QMutexLocker>

namespace NereusSDR {

MmioEndpoint::MmioEndpoint(QObject* parent)
    : QObject(parent)
{
}

MmioEndpoint::~MmioEndpoint() = default;

namespace {

// From Thetis MeterManager.cs:41428 [v2.10.3.15] — data bits default to
// 8; NereusSDR serial endpoints always open 8N1 (SerialEndpointWorker).
constexpr int kSerialDataBits = 8;

} // namespace

void MmioEndpoint::setGuid(const QUuid& g)
{
    m_guid = g;
    QMutexLocker locker(&m_outLock);
    // [original inline comment from MeterManager.cs:41468]
    // update the four char if guid is assigned, unlike ip and port
    m_fourChar.clear();
}

QString MmioEndpoint::fourChar() const
{
    QMutexLocker locker(&m_outLock);
    if (m_fourChar.isEmpty()) {
        if (m_transport == Transport::Serial) {
            // From Thetis MeterManager.cs:41461 [v2.10.3.15]
            m_fourChar = NereusSDR::fourChar(m_serialDevice,
                                             m_serialBaud + kSerialDataBits, m_guid);
        } else {
            // From Thetis MeterManager.cs:41396, :41444, :41484 [v2.10.3.15]
            m_fourChar = NereusSDR::fourChar(m_host, m_port, m_guid);
        }
    }
    return m_fourChar;
}

void MmioEndpoint::setFourChar(const QString& id)
{
    QMutexLocker locker(&m_outLock);
    m_fourChar = id;
}

QString MmioEndpoint::customTerminator() const
{
    QMutexLocker locker(&m_outLock);
    return m_customTerminator;
}

void MmioEndpoint::setCustomTerminator(const QString& t)
{
    QMutexLocker locker(&m_outLock);
    m_customTerminator = t;
}

QString MmioEndpoint::udpTargetHost() const
{
    QMutexLocker locker(&m_outLock);
    return m_udpTargetHost;
}

void MmioEndpoint::setUdpTargetHost(const QString& h)
{
    QMutexLocker locker(&m_outLock);
    m_udpTargetHost = h;
}

// From Thetis MeterManager.cs:41606-41615 [v2.10.3.15] — EnqueueOutbound.
// Thetis also returns when the endpoint is disabled; NereusSDR endpoints
// have no disabled state, so direction alone gates the queue.
bool MmioEndpoint::enqueueOutbound(const QString& message)
{
    if (direction() == Direction::In) {
        return false;
    }

    QMutexLocker locker(&m_outLock);
    // limit to a max of 1000. Not nice, but this will only happen if the sink/client is not available
    // and this mmio is enabled and we trying to send data
    if (m_outbound.size() >= kMaxOutbound) {
        m_outbound.dequeue();
    }

    m_outbound.enqueue(message);
    return true;
}

// From Thetis MeterManager.cs:41616-41623 [v2.10.3.15] — DequeueOutbound,
// taken up to max at a time.
QStringList MmioEndpoint::takeOutboundBatch(int max)
{
    QStringList batch;
    if (direction() == Direction::In) {
        return batch;
    }

    QMutexLocker locker(&m_outLock);
    while (!m_outbound.isEmpty() && batch.size() < max) {
        batch.append(m_outbound.dequeue());
    }
    return batch;
}

int MmioEndpoint::outboundQueueSize() const
{
    QMutexLocker locker(&m_outLock);
    return static_cast<int>(m_outbound.size());
}

QString MmioEndpoint::outboundTerminatorString() const
{
    // From Thetis MeterManager.cs:41991-42007 [v2.10.3.15] — TerminatorOut.
    switch (terminator()) {
    case Terminator::None:
        return QString();
    case Terminator::Cr:
        return QStringLiteral("\r");
    case Terminator::Lf:
        return QStringLiteral("\n");
    case Terminator::CrLf:
        return QStringLiteral("\r\n");
    case Terminator::Custom: {
        // From Thetis MeterManager.cs:41628-41643 [v2.10.3.15] —
        // CustomTerminatorOut / CustomTerminatorParsedOut.
        QString parsed = customTerminator();
        parsed.replace(QStringLiteral("\\n"), QStringLiteral("\n")); // use @ so that it is a literal verbatim string
        parsed.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
        parsed.replace(QStringLiteral("\\0"), QString(QChar(u'\0')));
        return parsed.isEmpty() ? QString(QChar(u'\0')) : parsed;
    }
    }
    return QString();
}

QByteArray MmioEndpoint::composeOutbound(const QStringList& batch,
                                         const QString& terminator)
{
    // From Thetis MeterManager.cs:42009-42020 [v2.10.3.15] — every message
    // is followed by the terminator, then the whole send is encoded as
    // ASCII. .NET Encoding.ASCII writes '?' for anything above 0x7F.
    QByteArray out;
    for (const QString& message : batch) {
        const QString piece = message + terminator;
        for (const QChar ch : piece) {
            out.append(ch.unicode() < 0x80 ? static_cast<char>(ch.unicode()) : '?');
        }
    }
    return out;
}

QByteArray MmioEndpoint::takeOutboundPayload()
{
    const QStringList batch = takeOutboundBatch(kSendBatch);
    if (batch.isEmpty()) {
        return QByteArray();
    }
    return composeOutbound(batch, outboundTerminatorString());
}

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
