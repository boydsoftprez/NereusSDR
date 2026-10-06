#pragma once
// no-port-check: NereusSDR-original. The Power Genius's and Tuner Genius's
// own settings, as the Core last read or set them, mirrored read-only as
// the `accessorySettings` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/AccessorySettingsModel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-47 / R-R3-22.
//
// The settings the amp and the tuner keep themselves (their name, the
// amp's bias, fan and LED, their network settings), as the Core last read
// them from the device or had them accepted by it, and the device's last
// answer to a window's request in plain words. The Core mirrors it as the
// read-only `accessorySettings` object (remotePgxlControlVersion 3 for the
// pgxl* properties, remoteTgxlControlVersion 1 for the tgxl* ones). A
// window changes a device setting only through the setPgxl* / setTgxl*,
// save and read-back commands; never by writing this object.
//
// The wire contract: docs/architecture/2026-09-23-remote-accessory-control-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariant>

namespace NereusSDR {

class NEREUS_CORE_EXPORT AccessorySettingsModel : public QObject {
    Q_OBJECT

    // The Power Genius. The answer text and its acceptance come before the
    // count: a window applies a delta in this order and shows the answer
    // when the count moves.
    Q_PROPERTY(QString pgxlNickname READ pgxlNickname NOTIFY pgxlChanged)
    Q_PROPERTY(QString pgxlBiasMode READ pgxlBiasMode NOTIFY pgxlChanged)
    Q_PROPERTY(QString pgxlFanMode READ pgxlFanMode NOTIFY pgxlChanged)
    Q_PROPERTY(int pgxlLedIntensity READ pgxlLedIntensity NOTIFY pgxlChanged)
    Q_PROPERTY(bool pgxlNetworkKnown READ pgxlNetworkKnown NOTIFY pgxlChanged)
    Q_PROPERTY(bool pgxlDhcp READ pgxlDhcp NOTIFY pgxlChanged)
    Q_PROPERTY(QString pgxlAddress READ pgxlAddress NOTIFY pgxlChanged)
    Q_PROPERTY(QString pgxlNetmask READ pgxlNetmask NOTIFY pgxlChanged)
    Q_PROPERTY(QString pgxlGateway READ pgxlGateway NOTIFY pgxlChanged)
    Q_PROPERTY(QString pgxlAnswer READ pgxlAnswer NOTIFY pgxlChanged)
    Q_PROPERTY(bool pgxlAnswerAccepted READ pgxlAnswerAccepted NOTIFY pgxlChanged)
    Q_PROPERTY(qint64 pgxlAnswerCount READ pgxlAnswerCount NOTIFY pgxlChanged)

    // The Tuner Genius (it has no bias, fan or LED setting).
    Q_PROPERTY(QString tgxlNickname READ tgxlNickname NOTIFY tgxlChanged)
    Q_PROPERTY(bool tgxlNetworkKnown READ tgxlNetworkKnown NOTIFY tgxlChanged)
    Q_PROPERTY(bool tgxlDhcp READ tgxlDhcp NOTIFY tgxlChanged)
    Q_PROPERTY(QString tgxlAddress READ tgxlAddress NOTIFY tgxlChanged)
    Q_PROPERTY(QString tgxlNetmask READ tgxlNetmask NOTIFY tgxlChanged)
    Q_PROPERTY(QString tgxlGateway READ tgxlGateway NOTIFY tgxlChanged)
    Q_PROPERTY(QString tgxlAnswer READ tgxlAnswer NOTIFY tgxlChanged)
    Q_PROPERTY(bool tgxlAnswerAccepted READ tgxlAnswerAccepted NOTIFY tgxlChanged)
    Q_PROPERTY(qint64 tgxlAnswerCount READ tgxlAnswerCount NOTIFY tgxlChanged)

public:
    /// One device's settings. Empty text and ledIntensity -1 mean the Core
    /// has not heard the value from the device; networkKnown is false until
    /// the device reported (or took) its network settings.
    struct Device {
        QString nickname;
        QString biasMode;       ///< "ClassA" or "ClassAB"; Power Genius only
        QString fanMode;        ///< "Auto", "Quiet" or "Continuous"; Power Genius only
        int ledIntensity{-1};   ///< 0 to 100; Power Genius only
        bool networkKnown{false};
        bool dhcp{false};
        QString address;
        QString netmask;
        QString gateway;
        QString answer;         ///< The device's last answer, in plain words
        bool answerAccepted{false};
        qint64 answerCount{0};  ///< Moves by one with each new answer
        bool operator==(const Device&) const = default;
    };

    /// Why a window cannot write this object: the Core refuses every write.
    static QString readOnlyReason();

    explicit AccessorySettingsModel(QObject* parent = nullptr);

    Device pgxl() const { return m_pgxl; }
    Device tgxl() const { return m_tgxl; }

    QString pgxlNickname() const { return m_pgxl.nickname; }
    QString pgxlBiasMode() const { return m_pgxl.biasMode; }
    QString pgxlFanMode() const { return m_pgxl.fanMode; }
    int pgxlLedIntensity() const { return m_pgxl.ledIntensity; }
    bool pgxlNetworkKnown() const { return m_pgxl.networkKnown; }
    bool pgxlDhcp() const { return m_pgxl.dhcp; }
    QString pgxlAddress() const { return m_pgxl.address; }
    QString pgxlNetmask() const { return m_pgxl.netmask; }
    QString pgxlGateway() const { return m_pgxl.gateway; }
    QString pgxlAnswer() const { return m_pgxl.answer; }
    bool pgxlAnswerAccepted() const { return m_pgxl.answerAccepted; }
    qint64 pgxlAnswerCount() const { return m_pgxl.answerCount; }

    QString tgxlNickname() const { return m_tgxl.nickname; }
    bool tgxlNetworkKnown() const { return m_tgxl.networkKnown; }
    bool tgxlDhcp() const { return m_tgxl.dhcp; }
    QString tgxlAddress() const { return m_tgxl.address; }
    QString tgxlNetmask() const { return m_tgxl.netmask; }
    QString tgxlGateway() const { return m_tgxl.gateway; }
    QString tgxlAnswer() const { return m_tgxl.answer; }
    bool tgxlAnswerAccepted() const { return m_tgxl.answerAccepted; }
    qint64 tgxlAnswerCount() const { return m_tgxl.answerCount; }

    // ---- The Core (or a test) ----
    void setPgxl(const Device& device);
    void setTgxl(const Device& device);

    /// A remote window: one of the Core's values arriving.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);

signals:
    void pgxlChanged();
    void tgxlChanged();

private:
    static bool applyDeviceValue(Device* device, const QByteArray& name, const QVariant& value);

    Device m_pgxl;
    Device m_tgxl;
};

} // namespace NereusSDR
