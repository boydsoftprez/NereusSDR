// no-port-check: NereusSDR-original POSIX PTY mechanics; no upstream code port.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "CatConfiguration.h"
#include <QObject>
#include <QTimer>
#include <QSocketNotifier>
#include <memory>
namespace NereusSDR {
class CatPtyTransport : public QObject {
    Q_OBJECT
public:
    explicit CatPtyTransport(QObject* parent = nullptr);
    ~CatPtyTransport() override;
    bool start(int channel, const CatEndpointConfig&);
    void stop();
    QString slavePath() const { return m_path; }
    QString errorString() const { return m_error; }
    bool isOpen() const;
    bool hasPeer() const { return m_peer; }
    bool attachSession(quint64 id);
    void closeSession(quint64 id);
    void writeBytes(const QByteArray& bytes) { writeBytes(m_sessionId, bytes); }
    bool writeBytes(quint64 id, const QByteArray&);
    qint64 queuedBytes() const { return m_pending.size(); }
signals:
    void peerOpened();
    void peerClosed(quint64 sessionId);
    void bytesReceived(QByteArray bytes);
    void failed(QString error);
private:
    struct Descriptor;
    bool observePeer();
    void readReady();
    void drain();
    void losePeer();
    void fail(const QString&);
    std::unique_ptr<Descriptor> m_master;
    std::shared_ptr<QSocketNotifier> m_readNotifier;
    std::shared_ptr<QSocketNotifier> m_writeNotifier;
    QTimer m_peerTimer;
    QString m_path;
    QString m_error;
    QByteArray m_pending;
    quint64 m_generation{0};
    quint64 m_sessionId{0};
    bool m_peer{false};
};
} // namespace NereusSDR
