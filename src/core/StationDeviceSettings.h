#pragma once
// no-port-check: NereusSDR-original. R-R3-47 / R-R3-22: a window's request
// for one of the Power Genius's or Tuner Genius's own settings, sent by the
// Core as the same device command a local window's Advanced page sends.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/StationDeviceSettings.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-47 / R-R3-22.
//
// The Core's half of the Advanced pages' device settings. Each request is
// sent as exactly the command the local page sends (PgxlAdvancedPage.cpp
// and TgxlAdvancedPage.cpp), through the connection's own methods:
//
//   name               setup nickname=<name>                  (writeSetup)
//   bias (amp)         setup bias=a | setup bias=ab           (writeSetup)
//   fan (amp)          setup fan=auto|quiet|continuous        (writeSetup)
//   LED (amp)          setup led=<0-100>                      (writeSetup)
//   network            ifconf address=<ip> netmask=<mask>
//                        gateway=<gw> dhcp=<true|false>       (writeIfconf)
//   Save & Reboot      save                                   (save)
//   Revert             setup read, then ifconf read           (readSetup,
//                                                              readIfconf)
//
// The wire forms are the connections' (PgxlConnection.cpp and
// TgxlConnection.cpp, "From FlexRadio wiki spec" and design section 6.4
// cites there). The device's answer (the R-frame with the request's
// sequence) is matched here and published on AccessorySettingsModel in
// plain words, with the values it took or reported. None of these
// commands keys a transmitter or puts the amp in operate.
//
// The settings the local page saves beside each command (PGXL_Nickname,
// PGXL_BiasMode, PGXL_FanMode, PGXL_LedIntensity, TGXL_Nickname) are saved
// on the Core the same way.
//
// M5: a request the device has not answered within kAnswerTimeoutMs (on a
// monotonic clock) is given up with a plain answer ("The <device> did not
// answer. Try again."), so "Waiting for its answer." never stays forever; a late
// answer to it is ignored.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  A fixed network setting needs an address
//                                    and a netmask; a request with no answer
//                                    times out (R-R3-47). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "models/AccessorySettingsModel.h"

#include <QHash>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>

#include <functional>

namespace NereusSDR {

class StationDeviceSettings final : public QObject {
    Q_OBJECT
public:
    enum class Device { Pgxl, Tgxl };

    /// The connection's own command methods. Each send returns the
    /// command's sequence, 0 when nothing was sent.
    struct Wire {
        std::function<bool()> connected;
        std::function<quint32(const QMap<QString, QString>&)> writeSetup;
        std::function<quint32()> readSetup;
        std::function<quint32(const QString&, const QString&, const QString&, bool)> writeIfconf;
        std::function<quint32()> readIfconf;
        std::function<quint32()> save;
    };

    /// M5: how long the Core waits for the device's answer to a request.
    static constexpr qint64 kAnswerTimeoutMs = 10000;

    StationDeviceSettings(Device device, Wire wire, QObject* parent = nullptr);

    /// M5, tests: the clock (ms) the Core reads to time requests, and the
    /// check its timer runs once a second while a request waits.
    void setClockForTesting(std::function<qint64()> now) { m_now = std::move(now); }
    qint64 clockNowForTesting() const { return m_now(); }
    void checkTimeouts();

    /// Where the Core publishes the device's settings and answers.
    void setModel(AccessorySettingsModel* model);

    // ---- Requests (each true when the command left for the device) ----
    bool setName(const QString& name, QString* reason);
    /// Power Genius only: "ClassA" or "ClassAB".
    bool setBiasMode(const QString& mode, QString* reason);
    /// Power Genius only: "Auto", "Quiet" or "Continuous".
    bool setFanMode(const QString& mode, QString* reason);
    /// Power Genius only: 0 to 100.
    bool setLedIntensity(int value, QString* reason);
    bool setNetwork(bool dhcp, const QString& address, const QString& netmask,
                    const QString& gateway, QString* reason);
    bool saveAndRestart(QString* reason);
    /// Revert: ask the device for its settings again.
    bool readBack(QString* reason);

    // ---- From the connection ----
    void onReply(quint32 seq, bool accepted, const QString& body);
    /// The device went away: a request still waiting gets no answer.
    void onDisconnected();
    /// A new scope (another address, radio or switch state): the values
    /// the Core heard belong to the device that is gone.
    void reset();

    /// Empty, or four numbers from 0 to 255 separated by dots (the local
    /// page's validator).
    static bool validNetworkField(const QString& text);

    /// I5: what is wrong with a network setting, in plain words, or empty
    /// when it may be sent. With DHCP off the device needs an address it
    /// can use and a netmask; a gateway, when given, must be on the same
    /// network as the address. The Core checks every request (it is the
    /// only gate for other apps); the local pages check the same way.
    static QString networkProblem(bool dhcp, const QString& address, const QString& netmask,
                                  const QString& gateway);

private:
    enum class Kind { Name, Bias, Fan, Led, Network, Save, ReadSetup, ReadNetwork };
    struct Pending {
        Kind kind{Kind::Name};
        QString value;
        int number{0};
        bool dhcp{false};
        QString address;
        QString netmask;
        QString gateway;
        qint64 sentAtMs{0};
    };

    QString deviceName() const;
    QString settingsPrefix() const;
    bool refuseIfOffline(QString* reason) const;
    bool sent(quint32 seq, const Pending& pending, QString* reason);
    AccessorySettingsModel::Device current() const;
    void publish(const AccessorySettingsModel::Device& next);
    void answer(AccessorySettingsModel::Device* next, const QString& text, bool accepted);
    static QMap<QString, QString> fieldsOf(const QString& body);
    static Pending pendingOf(Kind kind, const QString& value = QString());

    Device m_device;
    Wire m_wire;
    QPointer<AccessorySettingsModel> m_model;
    QHash<quint32, Pending> m_pending;
    // A monotonic clock (a Pi or Rock Core has no real-time clock, and its
    // wall clock steps at boot), counted from this object's start.
    QElapsedTimer m_monotonic;
    std::function<qint64()> m_now;
    QTimer m_timeoutTimer;
};

} // namespace NereusSDR
