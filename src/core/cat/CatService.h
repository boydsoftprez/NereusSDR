// --- From SerialPortPTT.cs ---
//=================================================================
// SerialPortPTT.cs
//=================================================================
// Copyright (C) 2005  Bill Tracey
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//=================================================================
// This class is used to implement a PTT using RTS or DTS 
//=================================================================

// --- From CATCommands.cs ---
//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
/*
Modifications to support the Behringer Midi controllers
by Chris Codella, W2PA, April 2017.  Indicated by //-W2PA comment lines.
Added extended CAT commands for APF funtions - May 2017.
*/
//=================================================================

// Ported from Thetis Project Files/Source/Console/CAT/SerialPortPTT.cs and CATCommands.cs
// Modification history (NereusSDR):
// 2026-10-04 - Composite release-armed input PTT and requesting serial close by
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
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
#include "CatSerialTransport.h"
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
    void applyPttSample(int channel, bool cts, bool dsr);
    QString pttState() const { return m_pttState; }
    CatSerialTransport* serialTransport(int channel) const;
    bool setSerialTransportFactoryForTest(std::function<std::shared_ptr<CatSerialTransport>()> factory);
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
    void pttStateChanged(QString state);
    void messageLogged(int channel, bool inbound, QByteArray bytes);
private:
    struct Channel {
        CatEndpointConfig config;
        bool configured{false};
        QString state{"Stopped"};
        std::shared_ptr<CatTcpTransport> tcp;
        std::shared_ptr<CatSerialTransport> serial;
    };
    struct PttInput {
        quint64 sessionId{0};
        int channel{1};
        CatBinding binding;
        bool useCts{false}; bool useDsr{false};
        bool armed{false}; bool asserted{false};
        std::shared_ptr<CatSerialTransport> transport;
        bool separate{false};
    };
    void startPtt();
    void stopPtt();
    void setPttState(const QString& state);
    void closeSerialChannel(int channel, const std::shared_ptr<CatSerialTransport>& transport);
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
    std::shared_ptr<PttInput> m_ptt;
    QString m_pttState{"Stopped"};
    std::function<std::shared_ptr<CatSerialTransport>()> m_serialFactory;
    quint64 m_nextSessionId{0};
    quint64 m_lifecycleGeneration{0};
    bool m_destroying{false};
    bool m_started{false};
};
} // namespace NereusSDR
