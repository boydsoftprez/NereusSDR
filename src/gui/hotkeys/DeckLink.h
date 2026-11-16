// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 19.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// The private link to deck plugins on this computer: one local socket per settings
// profile, open only to this user, carrying one JSON object per line. A plugin's presses
// go through the same dispatcher as every other source. Main thread only.
//
// A plugin can key the radio through this, so a hold never outlives its connection:
// every way a connection ends comes through one function that releases what it held.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

class QLocalServer;
class QLocalSocket;

namespace NereusSDR {

class HotKeyDispatcher;
class HotKeyStateFeed;

class NEREUS_GUI_EXPORT DeckLink : public QObject {
    Q_OBJECT

public:
    static constexpr int kProtocolVersion = 1;
    static constexpr qsizetype kMaxLineBytes = 65536;
    // A plugin that stops reading stops hearing, and if it holds a key it can no longer be
    // trusted to let go. Past this much unsent, its connection closes. Our choice: over a
    // thousand times the longest message that repeats.
    static constexpr qint64 kMaxUnsentBytes = 1024 * 1024;
    // The dispatcher runs a press-type action once per step on this thread. Our choice:
    // far past any dial tick, short of a stall.
    static constexpr int kMaxTurnSteps = 1000;

    DeckLink(HotKeyDispatcher* dispatcher, HotKeyStateFeed* feed, QObject* parent = nullptr);
    ~DeckLink() override;

    // Saves the choice to HotKeys/DeckLink/Enabled and opens or closes the link. False
    // means it could not open, and lastError says why. The choice stays saved: the next
    // launch tries again. Nothing listens until this is called.
    bool setEnabled(bool on);
    static bool savedEnabled();     // default on
    bool isListening() const;

    QString lastError() const { return m_lastError; }  // for the page
    QString socketPath() const;
    static QString deckFolder();    // <home>/.nereussdr/deck
    QList<QString> connectedPlugins() const;

signals:
    void pluginsChanged();
    void elgatoConnectionChanged(bool connected);
    void ulanziConnectionChanged(bool connected);

private:
    struct Connection {
        QString id;         // the source's instance: never reused, so a hold can't cross over
        QString plugin;     // empty until its hello
        QByteArray buffer;
    };

    bool startListening();
    void stopListening();
    bool writeDiscoveryFile(const QString& path);
    void acceptConnections();
    void readFrom(QLocalSocket* socket);
    void handleLine(QLocalSocket* socket, const QByteArray& line);
    void handleHello(QLocalSocket* socket, const QJsonObject& message);
    void handlePress(QLocalSocket* socket, const QJsonObject& message);
    void handleRelease(QLocalSocket* socket, const QJsonObject& message);
    void handleTurn(QLocalSocket* socket, const QJsonObject& message);
    void dropConnection(QLocalSocket* socket);
    void writeLine(QLocalSocket* socket, const QByteArray& line);
    void broadcast(const QByteArray& line);
    void onStateChanged(const QString& id, bool on);
    void onDisplayChanged();
    QByteArray displayLine() const;
    void noteHostsWithDevices();

    QPointer<HotKeyDispatcher> m_dispatcher;
    QPointer<HotKeyStateFeed> m_feed;
    std::unique_ptr<QLocalServer> m_server;
    QHash<QLocalSocket*, Connection> m_connections;
    QList<QLocalSocket*> m_droppedWhileReading;
    int m_readDepth{0};
    QString m_lastError;
    QString m_listenName;
    QString m_discoveryPath;
    QByteArray m_lastDisplayLine;
    quint64 m_lastConnectionId{0};
    bool m_elgatoConnected{false};
    bool m_ulanziConnected{false};
};

} // namespace NereusSDR
