// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "CatModelAdapter.h"
#include "CatTxCoordinator.h"
#include "CatSettings.h"
#include "CatRxCommands.h"
#include "CatDspCommands.h"
#include "CatTxCommands.h"
#include "CatGlobalCommands.h"
#include "CatSession.h"
#include "CatParser.h"
#include "CatCommandRouter.h"
#include "CatTcpTransport.h"
#include "CatReporter.h"
#include <QObject>
#include <QPointer>
#include <QHash>
#include <array>
#include <memory>
namespace NereusSDR {
class RadioModel;
class CatService : public QObject {
    Q_OBJECT
public:
    explicit CatService(RadioModel& model, QObject* parent = nullptr);
    ~CatService() override;
    bool applyChannelConfig(int, const CatEndpointConfig&);
    CatEndpointConfig channelConfig(int) const;
    void startConfigured();
    void stopAll();
    void beginRetirement();
    bool isStarted() const { return m_started; }
    bool isListening(int channel) const;
    QString channelState(int channel) const;
    CatGlobalConfig globalConfig() const;
    bool applyGlobalConfig(const CatGlobalConfig&);
    quint64 openSession(int channel, CatTransportKind);
    void closeSession(quint64);
    CatSession* session(quint64);
    QByteArray processFrame(quint64, const QByteArray&);
    void processBytes(quint64, const QByteArray&);
    void sendToSession(quint64, const QByteArray&);
    void sendToGuid(const QUuid&, const QByteArray&);
    QList<quint64> sessionIds(int channel) const;
    int clientCount(int channel) const;
    QHostAddress boundAddress(int channel) const;
    quint16 boundPort(int channel) const;
    CatReporter& reporter() { return *m_reporter; }
    CatTxCoordinator& txCoordinator() { return m_txCoordinator; }
    CatModelAdapter& adapter() { return m_adapter; }
    CatSettings& settings() { return m_settings; }
    CatCommandRouter& router() { return m_router; }
signals:
    void channelStateChanged(int channel, QString state);
    void configurationChanged(int channel);
    void globalConfigurationChanged();
    void sessionClosed(quint64 sessionId);
    void radioDisconnected();
    void clientCountChanged(int channel, int count);
    void messageLogged(int channel, bool inbound, QByteArray bytes);
private:
    struct Channel {
        CatEndpointConfig config;
        bool configured{false};
        QString state{"Stopped"};
        std::shared_ptr<CatTcpTransport> tcp;
    };
    void startChannel(int channel);
    void stopChannel(int channel);
    quint64 createTransportSession(int channel, CatTransportKind, std::function<bool(quint64, const QByteArray&)>, std::function<void(quint64)>);
    bool validChannel(int channel) const;
    void setState(int channel, const QString&);
    QPointer<RadioModel> m_model;
    CatModelAdapter m_adapter;
    CatTxCoordinator m_txCoordinator;
    CatSettings m_settings;
    CatRxCommands m_rxCommands;
    CatDspCommands m_dspCommands;
    CatTxCommands m_txCommands;
    CatGlobalCommands m_globalCommands;
    CatCommandCatalog m_catalog;
    CatParser m_parser;
    CatCommandRouter m_router;
    std::array<Channel, 4> m_channels;
    QHash<quint64, std::shared_ptr<CatSession>> m_sessions;
    std::unique_ptr<CatReporter> m_reporter;
    quint64 m_nextSessionId{0};
    quint64 m_lifecycleGeneration{0};
    bool m_destroying{false};
    bool m_started{false};
};
} // namespace NereusSDR
