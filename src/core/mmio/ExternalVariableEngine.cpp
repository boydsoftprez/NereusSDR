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
