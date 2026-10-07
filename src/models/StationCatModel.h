#pragma once
// no-port-check: NereusSDR-original. The Core's CAT setup and status as the
// mirrored, read-only `stationCat` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/StationCatModel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// What the Core's CAT is set up to do and doing: its four channels, its
// global settings, what the Core's computer offers for serial and PTY
// devices, and the reply to the latest test command. The Core mirrors it
// as the read-only `stationCat` object (stationCatVersion 1) to a peer
// whose hello declared stationCat 1; a connected desktop's CAT pages read
// it and change the Core's CAT only through the setStationCatChannel,
// setStationCatGlobal, testStationCatCommand and refreshStationCatDevices
// commands, never by writing this object.
//
// Each property is one UTF-8 JSON text, so one change is one delta:
//   global     {"config": {every CatGlobalConfig field by name},
//               "aiActive": bool, "pttState": text}
//   channelN   {"config": {every CatEndpointConfig field by name, the
//                binding as primarySliceId / secondarySliceId (-1: none)},
//               "primaryValid": bool, "secondaryValid": bool,
//               "status": {state, tcp, serial, pty, rigctld (each
//                transport's state text), tcpBoundAddress, tcpBoundPort,
//                rigctldBoundAddress, rigctldBoundPort, tcpClients,
//                rigctldClients, ptyPath}}
//   platform   {serial, pty, markSpaceParity, oneAndHalfStop: bool,
//               serialDevices: [text]}
//   lastTest   {requestId, channel, command, reply, accepted}
//
// The design: docs/architecture/2026-10-07-remote-cat-setup-plan.md. The
// wire contract: docs/architecture/2026-09-23-station-link-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, stationCatVersion 1).
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/cat/CatConfiguration.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>

#include <array>
#include <optional>

namespace NereusSDR {

class NEREUS_CORE_EXPORT StationCatModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString global READ global NOTIFY stateChanged)
    Q_PROPERTY(QString channel1 READ channel1 NOTIFY stateChanged)
    Q_PROPERTY(QString channel2 READ channel2 NOTIFY stateChanged)
    Q_PROPERTY(QString channel3 READ channel3 NOTIFY stateChanged)
    Q_PROPERTY(QString channel4 READ channel4 NOTIFY stateChanged)
    Q_PROPERTY(QString platform READ platform NOTIFY stateChanged)
    Q_PROPERTY(QString lastTest READ lastTest NOTIFY stateChanged)

public:
    /// The Core's CAT channels, 1 to kChannels.
    static constexpr int kChannels = 4;
    /// The `catLog` stream's capacity: the CAT log window's own limit.
    static constexpr int kLogCapacity = 10000;

    /// Why a window cannot write this object: the Core refuses every write.
    static QString readOnlyReason();

    // ---- The JSON shapes the object and the commands carry ----

    /// A channel's settings, the binding as slice ids only (-1: none).
    static QJsonObject channelConfigToJson(const CatEndpointConfig& config);
    /// `base` with every field `json` carries applied; nullopt when a field
    /// has the wrong type. The binding's incarnations are left as `base`
    /// has them: the Core resolves them.
    static std::optional<CatEndpointConfig> channelConfigFromJson(const QJsonObject& json,
                                                                  const CatEndpointConfig& base);
    static QJsonObject globalConfigToJson(const CatGlobalConfig& config);
    static std::optional<CatGlobalConfig> globalConfigFromJson(const QJsonObject& json,
                                                               const CatGlobalConfig& base);
    /// One JSON object as the compact UTF-8 text a property carries, and
    /// back (an empty object when the text is not one).
    static QString toText(const QJsonObject& object);
    static QJsonObject fromText(const QString& text);

    explicit StationCatModel(QObject* parent = nullptr);

    QString global() const { return m_global; }
    QString channel1() const { return m_channels[0]; }
    QString channel2() const { return m_channels[1]; }
    QString channel3() const { return m_channels[2]; }
    QString channel4() const { return m_channels[3]; }
    /// Channel 1 to kChannels; empty for any other.
    QString channel(int channel) const;
    QString platform() const { return m_platform; }
    QString lastTest() const { return m_lastTest; }

    /// The same, parsed.
    QJsonObject globalObject() const { return fromText(m_global); }
    QJsonObject channelObject(int channel) const { return fromText(this->channel(channel)); }
    QJsonObject platformObject() const { return fromText(m_platform); }
    QJsonObject lastTestObject() const { return fromText(m_lastTest); }

    /// The Core's publisher (or a test).
    void setGlobal(const QString& json);
    void setChannel(int channel, const QString& json);
    void setPlatform(const QString& json);
    void setLastTest(const QString& json);

    /// A remote window: one of the Core's values arriving.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);

signals:
    void stateChanged();

private:
    QString m_global;
    std::array<QString, kChannels> m_channels;
    QString m_platform;
    QString m_lastTest;
};

} // namespace NereusSDR
