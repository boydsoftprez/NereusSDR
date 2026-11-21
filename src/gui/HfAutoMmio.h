#pragma once
// NereusSDR-original: UDP JSON command client for the HF-Auto MMIO helper.
//
// The MMIO helper owns the tuner and its CAT server. Nereus sends Tuner Follow,
// the TX frequency, and the HF-Auto container's button commands here
// (127.0.0.1:12052). The helper translates them to the controller's own UI.
#include <QByteArray>
#include <QObject>
#include <QString>

class QUdpSocket;

namespace NereusSDR {

class HfAutoMmio : public QObject {
    Q_OBJECT
public:
    static HfAutoMmio& instance();

    void sendCommand(const QString& command);
    void sendCommand(const QString& command, int value);
    void setFollow(bool on);
    void setFrequency(qint64 hz);
    void sendStartupFrequency(qint64 hz);

private:
    explicit HfAutoMmio(QObject* parent = nullptr);
    void sendPayload(const QByteArray& payload);

    QUdpSocket* m_socket{nullptr};
};

} // namespace NereusSDR
