#pragma once
// no-port-check: NereusSDR-original. What the CAT pages, the CAT applet, the
// CAT log window and the status bar read and change, in either role.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/cat/CatControl.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// One CatControl per window (RadioModel::catControl()):
//   LocalCatControl   the window runs its own radio: CatService on this
//                     computer, answered at once, as the CAT pages always
//                     were.
//   RemoteCatControl  a connected desktop: the Core's CAT as the mirrored
//                     `stationCat` object (StationCatModel), changed through
//                     the stationCat commands on IStationLink, its log from
//                     the `catLog` record stream while something listens.
//
// A write's answer comes to its callback: at once for the local one, when
// the Core answers for the remote one. A refused write's reason is the
// words to show.
//
// The design: docs/architecture/2026-10-07-remote-cat-setup-plan.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, the client side). Review
//                                    fixes: one signal per kind of change,
//                                    explicit rebinds, the tester's reply
//                                    by command, the log's backlog, the
//                                    status bar's indicator.
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/cat/CatConfiguration.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <array>
#include <functional>
#include <optional>

namespace NereusSDR {

class CatService;
class IStationLink;
class RadioModel;
struct RecordBatch;

/// One channel's live state: each transport's state text, where its
/// listeners are bound, their clients, the PTY path, and whether the
/// channel's slice bindings still name live slices.
struct CatChannelStatus {
    QString state;
    QString tcp;
    QString serial;
    QString pty;
    QString rigctld;
    QString tcpBoundAddress;
    int tcpBoundPort{0};
    QString rigctldBoundAddress;
    int rigctldBoundPort{0};
    int tcpClients{0};
    int rigctldClients{0};
    QString ptyPath;
    /// Any of the channel's transports is open.
    bool listening{false};
    bool primaryValid{true};
    bool secondaryValid{true};
};

/// Which of a channel's slice bindings the operator just picked in a
/// selector: that slice id binds to its live slice now, even when it is the
/// id the channel already holds (a slice closed and opened again with the
/// same id). The local page carries the same choice in the live incarnation
/// it supplies; the Core is told it explicitly.
struct CatRebind {
    bool primary{false};
    bool secondary{false};
};

/// The status bar's CAT indicator: its text and its tooltip's lines.
struct CatIndicator {
    QString text;
    QStringList details;
};

/// What the computer running CAT offers.
struct CatPlatform {
    bool serial{false};
    bool pty{false};
    bool markSpaceParity{false};
    bool oneAndHalfStop{false};
    QStringList serialDevices;
};

class NEREUS_CORE_EXPORT CatControl : public QObject {
    Q_OBJECT
public:
    /// A write's answer: `accepted`, or the reason to show.
    using ResultCallback = std::function<void(bool accepted, const QString& reason)>;
    /// A test command's answer: `ran` with the CAT reply (empty: no
    /// reply), or the reason it did not run.
    using ReplyCallback =
        std::function<void(bool ran, const QByteArray& reply, const QString& reason)>;

    /// The lines the CAT log window keeps; a connected desktop's window is
    /// sent as many of the Core's recent lines when it opens.
    static constexpr int kLogLines = 10000;

    explicit CatControl(QObject* parent = nullptr) : QObject(parent) {}

    /// Channels 1 to 4.
    virtual CatEndpointConfig channelConfig(int channel) const = 0;
    virtual CatChannelStatus channelStatus(int channel) const = 0;
    virtual CatGlobalConfig globalConfig() const = 0;
    virtual QString pttState() const = 0;
    virtual CatPlatform platform() const = 0;
    /// Whether CAT can be read and changed from this window now, and why
    /// not when it cannot.
    virtual bool available() const = 0;
    virtual QString unavailableReason() const = 0;
    /// The CAT is the Core's, on another computer's terms.
    virtual bool remote() const = 0;

    /// `shownOn`: the page that shows a refusal itself, so the window does
    /// not also say it. `rebind`: the bindings the operator just picked.
    virtual void reconfigureChannel(int channel, const CatEndpointConfig& config,
                                    ResultCallback done, QObject* shownOn = nullptr,
                                    CatRebind rebind = {}) = 0;
    virtual void reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                                   QObject* shownOn = nullptr) = 0;
    virtual void testCommand(int channel, const QByteArray& command, ReplyCallback done,
                             QObject* shownOn = nullptr) = 0;
    /// Reads the serial devices again; platformChanged() follows when they
    /// changed.
    virtual void refreshDevices() = 0;

    /// The status bar's CAT indicator now.
    CatIndicator indicator() const;

signals:
    /// The global settings changed.
    void globalConfigChanged();
    /// A channel's settings changed.
    void channelConfigChanged(int channel);
    /// A channel's live state changed: a transport's state, where its
    /// listeners are bound, their clients, the PTY path, or whether its
    /// bindings still name live slices.
    void channelStatusChanged(int channel);
    void pttStateChanged();
    /// What the computer running CAT offers changed (its serial devices).
    void platformChanged();
    /// Whether CAT can be read and changed from this window changed.
    void availabilityChanged();
    /// One CAT exchange, at `timeMs` (milliseconds since the epoch).
    void logged(int channel, bool inbound, QByteArray bytes, qint64 timeMs);
};

/// The window's own CatService, answered at once.
class NEREUS_CORE_EXPORT LocalCatControl : public CatControl {
    Q_OBJECT
public:
    LocalCatControl(RadioModel* model, CatService* service, QObject* parent = nullptr);

    CatEndpointConfig channelConfig(int channel) const override;
    CatChannelStatus channelStatus(int channel) const override;
    CatGlobalConfig globalConfig() const override;
    QString pttState() const override;
    CatPlatform platform() const override;
    bool available() const override;
    QString unavailableReason() const override;
    bool remote() const override { return false; }
    /// `rebind` is carried by the live incarnation the page supplies.
    void reconfigureChannel(int channel, const CatEndpointConfig& config, ResultCallback done,
                            QObject* shownOn = nullptr, CatRebind rebind = {}) override;
    void reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                           QObject* shownOn = nullptr) override;
    void testCommand(int channel, const QByteArray& command, ReplyCallback done,
                     QObject* shownOn = nullptr) override;
    void refreshDevices() override;

private:
    QPointer<RadioModel> m_model;
    QPointer<CatService> m_service;
    QStringList m_serialDevices;
};

/// The Core's CAT, from a connected desktop.
class NEREUS_CORE_EXPORT RemoteCatControl : public CatControl {
    Q_OBJECT
public:
    explicit RemoteCatControl(RadioModel* model, QObject* parent = nullptr);

    /// Shown while the window is not connected to the Core.
    static QString notConnectedReason();

    CatEndpointConfig channelConfig(int channel) const override;
    CatChannelStatus channelStatus(int channel) const override;
    CatGlobalConfig globalConfig() const override;
    QString pttState() const override;
    CatPlatform platform() const override;
    bool available() const override;
    QString unavailableReason() const override;
    bool remote() const override { return true; }
    void reconfigureChannel(int channel, const CatEndpointConfig& config, ResultCallback done,
                            QObject* shownOn = nullptr, CatRebind rebind = {}) override;
    void reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                           QObject* shownOn = nullptr) override;
    void testCommand(int channel, const QByteArray& command, ReplyCallback done,
                     QObject* shownOn = nullptr) override;
    void refreshDevices() override;

    /// The `catLog` records the Core sent: each one not shown before is
    /// logged().
    void applyLogBatch(const RecordBatch& batch);
    /// The Core's answer to a test command this window sent: its reply.
    void applyTestReply(quint32 commandId, const QString& reply);
    /// Settings sent and still kept until the Core's answer and change
    /// arrive (0 once each is settled).
    int unconfirmedCount() const;

protected:
    void connectNotify(const QMetaMethod& signal) override;
    void disconnectNotify(const QMetaMethod& signal) override;

private:
    /// A setting sent and not in the mirror: the Core's answer and the
    /// change it makes come later, and a second change made meanwhile must
    /// build on the first, not on the older mirror.
    struct Unconfirmed {
        quint32 commandId{0};
        bool accepted{false};
    };
    /// The kept settings: channels 1 to 4 at 0 to 3, the global ones at 4.
    static constexpr int kGlobalSlot = 4;

    IStationLink* link() const;
    /// A command's outcome: refused now, or its answer awaited by id.
    void track(bool sent, const QString& reason, quint32 commandId, ResultCallback done,
               QObject* shownOn);
    /// The mirror changed: told once the whole delta is in (each property
    /// of a delta arrives on its own).
    void onStateChanged();
    void applyState();
    void onLinkChanged();
    void onCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    void followLog();
    void followLogLater();
    /// The mirror's config for a slot equals the setting kept for it.
    bool mirrorHolds(int slot) const;
    void drop(int slot);
    void dropAll();
    /// The Core answered a sent setting: the slot dropped, or -1.
    int settle(quint32 commandId, bool accepted);
    /// A delta for the slot's property arrived: true when its kept setting
    /// was dropped.
    bool settleOnDelta(int slot);
    void emitConfigChanged(int slot);

    QPointer<RadioModel> m_model;
    std::array<std::optional<CatEndpointConfig>, 4> m_sentChannel;
    std::optional<CatGlobalConfig> m_sentGlobal;
    std::array<Unconfirmed, 5> m_sentState;
    /// The mirror's objects last seen, so each change is told once, by kind.
    QJsonObject m_seenGlobal;
    std::array<QJsonObject, 4> m_seenChannel;
    QJsonObject m_seenPlatform;
    bool m_stateQueued{false};
    QHash<quint32, ResultCallback> m_pending;
    /// Test commands awaiting the Core's answer, by command id.
    QHash<quint32, ReplyCallback> m_tests;
    qint64 m_nextTestId{0};
    /// The link last told to follow (or leave) the `catLog` stream.
    IStationLink* m_logLink{nullptr};
    bool m_logFollowed{false};
    /// The newest `catLog` record shown, so a backlog sent again (after a
    /// new session) is not shown twice; a window opened again is sent the
    /// backlog afresh.
    quint64 m_logGeneration{0};
    qint64 m_lastLogId{0};
};

} // namespace NereusSDR
