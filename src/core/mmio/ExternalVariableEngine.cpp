// =================================================================
// src/core/mmio/ExternalVariableEngine.cpp  (NereusSDR)
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
//   2026-10-25 — MMIO outbound (container item parity): endpointByFourChar (GuidfromFourChar),
//                 enqueueOutbound (SendDataMMIO), and the new endpoint
//                 settings. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 — A backfilled 4-char ID's key is built from the same
//                 endpoint prefix as the other endpoint keys. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "ExternalVariableEngine.h"
#include "MmioEndpoint.h"
#include "ITransportWorker.h"
#include "UdpEndpointWorker.h"
#include "TcpListenerEndpointWorker.h"
#include "TcpClientEndpointWorker.h"
#ifdef HAVE_SERIALPORT
#include "SerialEndpointWorker.h"
#endif

#include "../LogCategories.h"
#include "../AppSettings.h"

#include <QThread>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QUuid>
#include <QString>
#include <QStringList>
#include <QLatin1String>
#include <QMetaObject>

namespace NereusSDR {

namespace {

constexpr const char* kSettingsPrefix = "MmioEndpoints/";

QString transportToString(MmioEndpoint::Transport t)
{
    switch (t) {
    case MmioEndpoint::Transport::UdpListener: return QStringLiteral("UdpListener");
    case MmioEndpoint::Transport::TcpListener: return QStringLiteral("TcpListener");
    case MmioEndpoint::Transport::TcpClient:   return QStringLiteral("TcpClient");
    case MmioEndpoint::Transport::Serial:      return QStringLiteral("Serial");
    }
    return QStringLiteral("UdpListener");
}

MmioEndpoint::Transport transportFromString(const QString& s)
{
    if (s == QLatin1String("TcpListener")) return MmioEndpoint::Transport::TcpListener;
    if (s == QLatin1String("TcpClient"))   return MmioEndpoint::Transport::TcpClient;
    if (s == QLatin1String("Serial"))      return MmioEndpoint::Transport::Serial;
    return MmioEndpoint::Transport::UdpListener;
}

QString formatToString(MmioEndpoint::Format f)
{
    switch (f) {
    case MmioEndpoint::Format::Json: return QStringLiteral("Json");
    case MmioEndpoint::Format::Xml:  return QStringLiteral("Xml");
    case MmioEndpoint::Format::Raw:  return QStringLiteral("Raw");
    }
    return QStringLiteral("Json");
}

MmioEndpoint::Format formatFromString(const QString& s)
{
    if (s == QLatin1String("Xml")) return MmioEndpoint::Format::Xml;
    if (s == QLatin1String("Raw")) return MmioEndpoint::Format::Raw;
    return MmioEndpoint::Format::Json;
}

QString directionToString(MmioEndpoint::Direction d)
{
    switch (d) {
    case MmioEndpoint::Direction::In:   return QStringLiteral("In");
    case MmioEndpoint::Direction::Out:  return QStringLiteral("Out");
    case MmioEndpoint::Direction::Both: return QStringLiteral("Both");
    }
    return QStringLiteral("In");
}

// A missing or unknown value is In: endpoints saved before the setting
// existed only received data.
MmioEndpoint::Direction directionFromString(const QString& s)
{
    if (s == QLatin1String("Out"))  { return MmioEndpoint::Direction::Out; }
    if (s == QLatin1String("Both")) { return MmioEndpoint::Direction::Both; }
    return MmioEndpoint::Direction::In;
}

QString terminatorToString(MmioEndpoint::Terminator t)
{
    switch (t) {
    case MmioEndpoint::Terminator::None:   return QStringLiteral("None");
    case MmioEndpoint::Terminator::Cr:     return QStringLiteral("CR");
    case MmioEndpoint::Terminator::Lf:     return QStringLiteral("LF");
    case MmioEndpoint::Terminator::CrLf:   return QStringLiteral("CRLF");
    case MmioEndpoint::Terminator::Custom: return QStringLiteral("Custom");
    }
    return QStringLiteral("None");
}

MmioEndpoint::Terminator terminatorFromString(const QString& s)
{
    if (s == QLatin1String("CR"))     { return MmioEndpoint::Terminator::Cr; }
    if (s == QLatin1String("LF"))     { return MmioEndpoint::Terminator::Lf; }
    if (s == QLatin1String("CRLF"))   { return MmioEndpoint::Terminator::CrLf; }
    if (s == QLatin1String("Custom")) { return MmioEndpoint::Terminator::Custom; }
    return MmioEndpoint::Terminator::None;
}

} // namespace

ExternalVariableEngine& ExternalVariableEngine::instance()
{
    static ExternalVariableEngine eng;
    return eng;
}

ExternalVariableEngine::ExternalVariableEngine()
    : QObject(nullptr)
{
    m_workerThread = new QThread();
    m_workerThread->setObjectName(QStringLiteral("MmioWorker"));
    m_workerThread->start();
}

ExternalVariableEngine::~ExternalVariableEngine()
{
    shutdown();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
}

void ExternalVariableEngine::init()
{
    loadFromSettings();
    qCInfo(lcMmio) << "ExternalVariableEngine initialized with"
                    << m_endpoints.size() << "endpoints";
}

void ExternalVariableEngine::shutdown()
{
    QWriteLocker locker(&m_registryLock);
    for (auto it = m_workers.begin(); it != m_workers.end(); ++it) {
        ITransportWorker* w = it.value();
        if (w) {
            QMetaObject::invokeMethod(w, "stop", Qt::BlockingQueuedConnection);
            w->deleteLater();
        }
    }
    m_workers.clear();
    qDeleteAll(m_endpoints);
    m_endpoints.clear();
}

QList<MmioEndpoint*> ExternalVariableEngine::endpoints() const
{
    QReadLocker locker(&m_registryLock);
    return m_endpoints.values();
}

MmioEndpoint* ExternalVariableEngine::endpoint(const QUuid& guid) const
{
    QReadLocker locker(&m_registryLock);
    return m_endpoints.value(guid, nullptr);
}

MmioEndpoint* ExternalVariableEngine::endpointByFourChar(const QString& id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }
    QReadLocker locker(&m_registryLock);
    for (MmioEndpoint* ep : m_endpoints) {
        if (ep->fourChar() == id) {
            return ep;
        }
    }
    return nullptr;
}

void ExternalVariableEngine::enqueueOutbound(const QUuid& guid, const QString& message)
{
    QReadLocker locker(&m_registryLock);
    MmioEndpoint* ep = m_endpoints.value(guid, nullptr);
    if (!ep || !ep->enqueueOutbound(message)) {
        return;
    }
    ITransportWorker* worker = m_workers.value(guid, nullptr);
    if (worker) {
        QMetaObject::invokeMethod(worker, "flushOutbound", Qt::QueuedConnection);
    }
}

void ExternalVariableEngine::addEndpoint(MmioEndpoint* ep)
{
    if (!ep) { return; }
    if (ep->guid().isNull()) {
        ep->setGuid(QUuid::createUuid());
    }

    {
        QWriteLocker locker(&m_registryLock);
        ep->setParent(this);
        m_endpoints.insert(ep->guid(), ep);
    }

    saveEndpointToSettings(ep);
    startWorker(ep);
    emit endpointAdded(ep->guid());
    qCInfo(lcMmio) << "Added MMIO endpoint" << ep->name() << ep->guid();
}

void ExternalVariableEngine::removeEndpoint(const QUuid& guid)
{
    MmioEndpoint* ep = nullptr;
    {
        QWriteLocker locker(&m_registryLock);
        ep = m_endpoints.take(guid);
    }
    if (!ep) { return; }

    stopWorker(guid);
    removeEndpointFromSettings(guid);
    ep->deleteLater();
    emit endpointRemoved(guid);
    qCInfo(lcMmio) << "Removed MMIO endpoint" << guid;
}

void ExternalVariableEngine::updateEndpoint(MmioEndpoint* ep)
{
    if (!ep) { return; }
    stopWorker(ep->guid());
    saveEndpointToSettings(ep);
    startWorker(ep);
    emit endpointChanged(ep->guid());
}

ITransportWorker* ExternalVariableEngine::makeWorker(MmioEndpoint* ep)
{
    if (!ep) { return nullptr; }

    switch (ep->transport()) {
    case MmioEndpoint::Transport::UdpListener:
        return new UdpEndpointWorker(ep);
    case MmioEndpoint::Transport::TcpListener:
        return new TcpListenerEndpointWorker(ep);
    case MmioEndpoint::Transport::TcpClient:
        return new TcpClientEndpointWorker(ep);
    case MmioEndpoint::Transport::Serial:
#ifdef HAVE_SERIALPORT
        return new SerialEndpointWorker(ep);
#else
        qCWarning(lcMmio) << "Serial transport requested but Qt6::SerialPort "
                            "not available at build time; endpoint"
                          << ep->name() << "will not start";
        return nullptr;
#endif
    }
    return nullptr;
}

void ExternalVariableEngine::startWorker(MmioEndpoint* ep)
{
    if (!ep) { return; }
    ITransportWorker* worker = makeWorker(ep);
    if (!worker) { return; }

    worker->moveToThread(m_workerThread);
    connect(worker, &ITransportWorker::valueBatchReceived,
            this,   &ExternalVariableEngine::onValueBatchReceived);

    {
        QWriteLocker locker(&m_registryLock);
        m_workers.insert(ep->guid(), worker);
    }
    QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection);
}

void ExternalVariableEngine::stopWorker(const QUuid& guid)
{
    ITransportWorker* worker = nullptr;
    {
        QWriteLocker locker(&m_registryLock);
        worker = m_workers.take(guid);
    }
    if (!worker) { return; }
    QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
    worker->deleteLater();
}

void ExternalVariableEngine::onValueBatchReceived(const QHash<QString, QVariant>& batch)
{
    // Route the batch to whichever endpoint the keys belong to.
    // Keys are prefixed "<guidStr>.<path>", so we strip the prefix
    // off one key to identify the owning endpoint.
    if (batch.isEmpty()) { return; }
    const QString firstKey = batch.constBegin().key();
    const int dot = firstKey.indexOf(QLatin1Char('.'));
    if (dot <= 0) { return; }
    const QString guidStr = firstKey.left(dot);
    const QUuid guid(guidStr);
    if (guid.isNull()) { return; }

    MmioEndpoint* ep = endpoint(guid);
    if (!ep) { return; }
    ep->mergeBatch(batch);
}

void ExternalVariableEngine::loadFromSettings()
{
    auto& s = AppSettings::instance();
    const QStringList allKeys = s.allKeys();
    // Group keys by guid under MmioEndpoints/<guid>/*.
    QMap<QString, QMap<QString, QString>> grouped;
    for (const QString& key : allKeys) {
        if (!key.startsWith(QLatin1String(kSettingsPrefix))) { continue; }
        const QString rest = key.mid(QLatin1String(kSettingsPrefix).size());
        const int slash = rest.indexOf(QLatin1Char('/'));
        if (slash <= 0) { continue; }
        const QString guidStr = rest.left(slash);
        const QString prop    = rest.mid(slash + 1);
        grouped[guidStr].insert(prop, s.value(key).toString());
    }

    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        const QUuid guid(it.key());
        if (guid.isNull()) { continue; }
        const auto& props = it.value();
        auto* ep = new MmioEndpoint(this);
        ep->setGuid(guid);
        ep->setName(props.value(QStringLiteral("Name")));
        ep->setTransport(transportFromString(props.value(QStringLiteral("Transport"))));
        ep->setFormat(formatFromString(props.value(QStringLiteral("Format"))));
        ep->setHost(props.value(QStringLiteral("Host"), QStringLiteral("0.0.0.0")));
        ep->setPort(static_cast<quint16>(props.value(QStringLiteral("Port"), QStringLiteral("0")).toUInt()));
        ep->setSerialDevice(props.value(QStringLiteral("Device")));
        ep->setSerialBaud(props.value(QStringLiteral("Baud"), QStringLiteral("9600")).toInt());
        ep->setDirection(directionFromString(props.value(QStringLiteral("Direction"))));
        ep->setTerminator(terminatorFromString(props.value(QStringLiteral("Terminator"))));
        ep->setCustomTerminator(props.value(QStringLiteral("CustomTerminator")));
        ep->setUdpTargetHost(props.value(QStringLiteral("UdpTargetHost"), QStringLiteral("127.0.0.1")));
        ep->setUdpTargetPort(static_cast<quint16>(
            props.value(QStringLiteral("UdpTargetPort"), QStringLiteral("10000")).toUInt()));
        const QString storedId = props.value(QStringLiteral("FourChar"));
        if (storedId.isEmpty()) {
            // An endpoint saved before IDs existed gets one now, from
            // its guid, host and port, and keeps it from here on.
            // Built like saveEndpointToSettings()'s keys, from the same
            // "MmioEndpoints/<guid>/" prefix.
            const QString prefix = QStringLiteral("%1%2/")
                .arg(QLatin1String(kSettingsPrefix), it.key());
            s.setValue(prefix + QStringLiteral("FourChar"), ep->fourChar());
        } else {
            ep->setFourChar(storedId);
        }

        {
            QWriteLocker locker(&m_registryLock);
            m_endpoints.insert(guid, ep);
        }
        startWorker(ep);
    }
}

void ExternalVariableEngine::saveEndpointToSettings(const MmioEndpoint* ep)
{
    if (!ep) { return; }
    auto& s = AppSettings::instance();
    const QString prefix = QStringLiteral("%1%2/")
        .arg(QLatin1String(kSettingsPrefix), ep->guid().toString(QUuid::WithoutBraces));
    s.setValue(prefix + QStringLiteral("Name"),      ep->name());
    s.setValue(prefix + QStringLiteral("Transport"), transportToString(ep->transport()));
    s.setValue(prefix + QStringLiteral("Format"),    formatToString(ep->format()));
    s.setValue(prefix + QStringLiteral("Host"),      ep->host());
    s.setValue(prefix + QStringLiteral("Port"),      QString::number(ep->port()));
    s.setValue(prefix + QStringLiteral("Device"),    ep->serialDevice());
    s.setValue(prefix + QStringLiteral("Baud"),      QString::number(ep->serialBaud()));
    s.setValue(prefix + QStringLiteral("Direction"), directionToString(ep->direction()));
    s.setValue(prefix + QStringLiteral("Terminator"), terminatorToString(ep->terminator()));
    s.setValue(prefix + QStringLiteral("CustomTerminator"), ep->customTerminator());
    s.setValue(prefix + QStringLiteral("UdpTargetHost"), ep->udpTargetHost());
    s.setValue(prefix + QStringLiteral("UdpTargetPort"), QString::number(ep->udpTargetPort()));
    s.setValue(prefix + QStringLiteral("FourChar"),  ep->fourChar());
}

void ExternalVariableEngine::removeEndpointFromSettings(const QUuid& guid)
{
    auto& s = AppSettings::instance();
    const QString prefix = QStringLiteral("%1%2/")
        .arg(QLatin1String(kSettingsPrefix), guid.toString(QUuid::WithoutBraces));
    const QStringList keys = s.allKeys();
    for (const QString& k : keys) {
        if (k.startsWith(prefix)) { s.remove(k); }
    }
}

} // namespace NereusSDR
