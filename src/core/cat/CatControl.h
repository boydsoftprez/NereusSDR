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
//                                    desktop, the client side).
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/cat/CatConfiguration.h"

#include <QByteArray>
#include <QHash>
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
    /// not also say it.
    virtual void reconfigureChannel(int channel, const CatEndpointConfig& config,
                                    ResultCallback done, QObject* shownOn = nullptr) = 0;
    virtual void reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                                   QObject* shownOn = nullptr) = 0;
    virtual void testCommand(int channel, const QByteArray& command, ReplyCallback done) = 0;
    /// Reads the serial devices again; changed() follows.
    virtual void refreshDevices() = 0;

signals:
    void changed();
    void logged(int channel, bool inbound, QByteArray bytes);
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
    void reconfigureChannel(int channel, const CatEndpointConfig& config, ResultCallback done,
                            QObject* shownOn = nullptr) override;
    void reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                           QObject* shownOn = nullptr) override;
    void testCommand(int channel, const QByteArray& command, ReplyCallback done) override;
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
                            QObject* shownOn = nullptr) override;
    void reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                           QObject* shownOn = nullptr) override;
    void testCommand(int channel, const QByteArray& command, ReplyCallback done) override;
    void refreshDevices() override;

    /// The `catLog` records the Core sent: each one is logged().
    void applyLogBatch(const RecordBatch& batch);

protected:
    void connectNotify(const QMetaMethod& signal) override;
    void disconnectNotify(const QMetaMethod& signal) override;

private:
    IStationLink* link() const;
    /// A command's outcome: refused now, or its answer awaited by id.
    void track(bool sent, const QString& reason, quint32 commandId, ResultCallback done,
               QObject* shownOn);
    void onStateChanged();
    void onLinkChanged();
    void onCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    void followLog();
    void followLogLater();
    /// The Core answered a sent setting.
    void settle(quint32 commandId, bool accepted);

    /// A setting sent and not in the mirror: the Core's answer and the
    /// change it makes come later, and a second change made meanwhile must
    /// build on the first, not on the older mirror.
    struct Unconfirmed {
        quint32 commandId{0};
        bool accepted{false};
    };

    QPointer<RadioModel> m_model;
    std::array<std::optional<CatEndpointConfig>, 4> m_sentChannel;
    std::array<Unconfirmed, 4> m_sentChannelState;
    std::optional<CatGlobalConfig> m_sentGlobal;
    Unconfirmed m_sentGlobalState;
    QHash<quint32, ResultCallback> m_pending;
    QHash<qint64, ReplyCallback> m_tests;
    qint64 m_nextTestId{0};
    /// The link last told to follow (or leave) the `catLog` stream.
    IStationLink* m_logLink{nullptr};
    bool m_logFollowed{false};
};

} // namespace NereusSDR
