// NereusSDR-original: HF-Auto MMIO helper command client.
#include "HfAutoMmio.h"
#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUdpSocket>

namespace NereusSDR {
namespace {
constexpr quint16 kHfAutoCommandPort = 12052;
}

HfAutoMmio& HfAutoMmio::instance()
{
    static HfAutoMmio* client = new HfAutoMmio(QCoreApplication::instance());
    return *client;
}

HfAutoMmio::HfAutoMmio(QObject* parent) : QObject(parent)
{
    m_socket = new QUdpSocket(this);
}

void HfAutoMmio::sendCommand(const QString& command)
{
    QJsonObject object;
    object.insert(QStringLiteral("command"), command);
    sendPayload(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void HfAutoMmio::sendCommand(const QString& command, int value)
{
    QJsonObject object;
    object.insert(QStringLiteral("command"), command);
    object.insert(QStringLiteral("value"), value);
    sendPayload(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void HfAutoMmio::setFollow(bool on)
{
    sendCommand(QStringLiteral("tf"), on ? 1 : 0);
}

void HfAutoMmio::setFrequency(qint64 hz)
{
    if (hz <= 0) { return; }
    // Send the frequency as an integer literal: CommanderServer reads the
    // value with GetInt32(), and 54 MHz is well inside its range.
    const QByteArray payload =
        QStringLiteral("{\"command\":\"frequency\",\"value\":%1}").arg(hz).toUtf8();
    sendPayload(payload);
}

void HfAutoMmio::sendStartupFrequency(qint64 hz)
{
    if (hz <= 0) { return; }
    const QByteArray payload =
        QStringLiteral("{\"command\":\"startup\",\"value\":%1}").arg(hz).toUtf8();
    sendPayload(payload);
}

void HfAutoMmio::sendPayload(const QByteArray& payload)
{
    if (!m_socket) { return; }
    m_socket->writeDatagram(payload, QHostAddress::LocalHost, kHfAutoCommandPort);
}

} // namespace NereusSDR
