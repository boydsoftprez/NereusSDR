// no-port-check: NereusSDR-original. See StationDeviceSettings.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/StationDeviceSettings.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  A fixed network setting needs an address
//                                    and a netmask (R-R3-47). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  A fixed setting refuses a /32 netmask and
//                                    the subnet's network and broadcast
//                                    addresses (R-R3-47). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  A gateway at the subnet's network or
//                                    broadcast address is refused (R-R3-47).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  A request with no answer times out
//                                    (R-R3-47), on a monotonic clock.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix wave RD-I11: setName refuses a name with a space or
//               '=' (it would add fields to the setup line). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/StationDeviceSettings.h"

#include "core/PgxlConnection.h"

#include "core/AppSettings.h"

#include <QHostAddress>
#include <QRegularExpression>

namespace NereusSDR {

namespace {
// The local pages' address validator (PgxlAdvancedPage.cpp and
// TgxlAdvancedPage.cpp, buildNetworkSection).
const QRegularExpression& addressPattern()
{
    static const QRegularExpression ipRe(
        QStringLiteral(
            "^(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$"));
    return ipRe;
}

const QStringList& fanModes()
{
    // The local page's fan mode combo (PgxlAdvancedPage::buildHardwareSection).
    static const QStringList modes{QStringLiteral("Auto"), QStringLiteral("Quiet"),
                                   QStringLiteral("Continuous")};
    return modes;
}
} // namespace

StationDeviceSettings::StationDeviceSettings(Device device, Wire wire, QObject* parent)
    : QObject(parent), m_device(device), m_wire(std::move(wire))
    , m_now([this] { return m_monotonic.elapsed(); })
{
    m_monotonic.start();
    m_timeoutTimer.setInterval(1000);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &StationDeviceSettings::checkTimeouts);
}

void StationDeviceSettings::checkTimeouts()
{
    const qint64 now = m_now();
    bool expired = false;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (now - it.value().sentAtMs >= kAnswerTimeoutMs) {
            it = m_pending.erase(it);
            expired = true;
        } else {
            ++it;
        }
    }
    if (m_pending.isEmpty()) {
        m_timeoutTimer.stop();
    }
    if (expired) {
        AccessorySettingsModel::Device next = current();
        answer(&next, QStringLiteral("The %1 did not answer. Try again.").arg(deviceName()),
               false);
        publish(next);
    }
}

void StationDeviceSettings::setModel(AccessorySettingsModel* model)
{
    m_model = model;
}

QString StationDeviceSettings::deviceName() const
{
    return m_device == Device::Pgxl ? QStringLiteral("Power Genius")
                                    : QStringLiteral("Tuner Genius");
}

QString StationDeviceSettings::settingsPrefix() const
{
    return m_device == Device::Pgxl ? QStringLiteral("PGXL_") : QStringLiteral("TGXL_");
}

bool StationDeviceSettings::validNetworkField(const QString& text)
{
    return text.isEmpty() || addressPattern().match(text).hasMatch();
}

QString StationDeviceSettings::networkProblem(bool dhcp, const QString& inputAddress,
                                              const QString& inputNetmask,
                                              const QString& inputGateway)
{
    const QString address = inputAddress.trimmed();
    const QString netmask = inputNetmask.trimmed();
    const QString gateway = inputGateway.trimmed();
    if (!validNetworkField(address) || !validNetworkField(netmask)
        || !validNetworkField(gateway)) {
        return QStringLiteral("Enter each address as four numbers from 0 to 255 "
                              "separated by dots.");
    }
    if (dhcp) {
        return {};
    }
    if (address.isEmpty() || netmask.isEmpty()) {
        return QStringLiteral("Without DHCP, enter an address and a netmask.");
    }
    const quint32 mask = QHostAddress(netmask).toIPv4Address();
    // A netmask is ones, then zeros (255.255.255.0), not all zeros, and not
    // /32 (a device alone on its network reaches nothing).
    if (mask == 0 || mask == 0xFFFFFFFFu || ((~mask) & ((~mask) + 1u)) != 0) {
        return QStringLiteral("Enter a netmask such as 255.255.255.0.");
    }
    const QHostAddress host(address);
    const quint32 ip = host.toIPv4Address();
    // Nor the subnet's network or broadcast address (except on a /31
    // point-to-point link, where both are hosts).
    const bool pointToPoint = mask == 0xFFFFFFFEu;
    const bool networkOrBroadcast = !pointToPoint
        && ((ip & ~mask) == 0 || (ip | mask) == 0xFFFFFFFFu);
    if (ip == 0 || host.isLoopback() || host.isMulticast() || ip == 0xFFFFFFFFu
        || (ip >> 24) >= 240u || networkOrBroadcast) {
        return QStringLiteral("Enter an address the device can use on your network.");
    }
    if (!gateway.isEmpty()) {
        const quint32 gw = QHostAddress(gateway).toIPv4Address();
        // Rework part 6: nor at the subnet's network or broadcast address
        // (except on a /31 link).
        const bool gwNetworkOrBroadcast = !pointToPoint
            && ((gw & ~mask) == 0 || (gw | mask) == 0xFFFFFFFFu);
        if ((gw & mask) != (ip & mask) || gw == ip || gwNetworkOrBroadcast) {
            return QStringLiteral("Enter a gateway on the same network as the address, "
                                  "or leave it empty.");
        }
    }
    return {};
}

AccessorySettingsModel::Device StationDeviceSettings::current() const
{
    if (!m_model) {
        return {};
    }
    return m_device == Device::Pgxl ? m_model->pgxl() : m_model->tgxl();
}

void StationDeviceSettings::publish(const AccessorySettingsModel::Device& next)
{
    if (!m_model) {
        return;
    }
    if (m_device == Device::Pgxl) {
        m_model->setPgxl(next);
    } else {
        m_model->setTgxl(next);
    }
}

void StationDeviceSettings::answer(AccessorySettingsModel::Device* next, const QString& text,
                                   bool accepted)
{
    next->answer = text;
    next->answerAccepted = accepted;
    ++next->answerCount;
}

bool StationDeviceSettings::refuseIfOffline(QString* reason) const
{
    if (m_wire.connected && m_wire.connected()) {
        return false;
    }
    if (reason) {
        *reason = QStringLiteral("The Core is not connected to the %1.").arg(deviceName());
    }
    return true;
}

bool StationDeviceSettings::sent(quint32 seq, const Pending& pending, QString* reason)
{
    if (seq == 0) {
        if (reason) {
            *reason = QStringLiteral("The Core is not connected to the %1.").arg(deviceName());
        }
        return false;
    }
    Pending timed = pending;
    timed.sentAtMs = m_now();
    m_pending.insert(seq, timed);
    if (!m_timeoutTimer.isActive()) {
        m_timeoutTimer.start();
    }
    AccessorySettingsModel::Device next = current();
    answer(&next,
           QStringLiteral("Sent to the %1. Waiting for its answer.").arg(deviceName()), true);
    publish(next);
    if (reason) {
        reason->clear();
    }
    return true;
}

bool StationDeviceSettings::setName(const QString& input, QString* reason)
{
    const QString name = input.trimmed();
    for (const QChar c : name) {
        if (c.category() == QChar::Other_Control) {
            if (reason) {
                *reason = QStringLiteral("Enter a name without line breaks or tabs.");
            }
            return false;
        }
    }
    // RD-I11: the name goes out as one `nickname=` field of a
    // space-separated `setup` line, and the amp reports it back as one
    // word (its discovery line, LanDiscovery). A space or an equals sign
    // would end the field and start another (`Shack bias=a`), so neither
    // is sent.
    if (!PgxlConnection::isSetupToken(name)) {
        if (reason) {
            *reason = QStringLiteral("Enter a name without spaces or equals signs.");
        }
        return false;
    }
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From PgxlAdvancedPage / TgxlAdvancedPage buildIdentitySection: the
    // name is written with `setup nickname=` and saved beside it.
    const quint32 seq = m_wire.writeSetup({{QStringLiteral("nickname"), name}});
    if (!sent(seq, pendingOf(Kind::Name, name), reason)) {
        return false;
    }
    AppSettings::instance().setValue(settingsPrefix() + QStringLiteral("Nickname"), name);
    return true;
}

bool StationDeviceSettings::setBiasMode(const QString& mode, QString* reason)
{
    if (m_device != Device::Pgxl
        || (mode != QLatin1String("ClassA") && mode != QLatin1String("ClassAB"))) {
        if (reason) {
            *reason = QStringLiteral("Choose Class A or Class AB.");
        }
        return false;
    }
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From PgxlAdvancedPage::onBiasModeChanged: `setup bias=a|ab`.
    const QString wire = mode == QLatin1String("ClassA") ? QStringLiteral("a")
                                                        : QStringLiteral("ab");
    const quint32 seq = m_wire.writeSetup({{QStringLiteral("bias"), wire}});
    if (!sent(seq, pendingOf(Kind::Bias, mode), reason)) {
        return false;
    }
    AppSettings::instance().setValue(QStringLiteral("PGXL_BiasMode"), mode);
    return true;
}

bool StationDeviceSettings::setFanMode(const QString& mode, QString* reason)
{
    if (m_device != Device::Pgxl || !fanModes().contains(mode)) {
        if (reason) {
            *reason = QStringLiteral("Choose Auto, Quiet or Continuous.");
        }
        return false;
    }
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From PgxlAdvancedPage::onFanModeChanged: `setup fan=<lower case>`.
    const quint32 seq = m_wire.writeSetup({{QStringLiteral("fan"), mode.toLower()}});
    if (!sent(seq, pendingOf(Kind::Fan, mode), reason)) {
        return false;
    }
    AppSettings::instance().setValue(QStringLiteral("PGXL_FanMode"), mode);
    return true;
}

bool StationDeviceSettings::setLedIntensity(int value, QString* reason)
{
    // The local page's slider range (buildHardwareSection).
    if (m_device != Device::Pgxl || value < 0 || value > 100) {
        if (reason) {
            *reason = QStringLiteral("Choose an LED brightness from 0 to 100.");
        }
        return false;
    }
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From PgxlAdvancedPage::onLedSliderChanged: `setup led=<value>`.
    const quint32 seq = m_wire.writeSetup({{QStringLiteral("led"), QString::number(value)}});
    Pending pending = pendingOf(Kind::Led);
    pending.number = value;
    if (!sent(seq, pending, reason)) {
        return false;
    }
    AppSettings::instance().setValue(QStringLiteral("PGXL_LedIntensity"), value);
    return true;
}

bool StationDeviceSettings::setNetwork(bool dhcp, const QString& inputAddress,
                                       const QString& inputNetmask,
                                       const QString& inputGateway, QString* reason)
{
    const QString address = inputAddress.trimmed();
    const QString netmask = inputNetmask.trimmed();
    const QString gateway = inputGateway.trimmed();
    // I5: the Core is the only gate for every app that sends this.
    const QString problem = networkProblem(dhcp, address, netmask, gateway);
    if (!problem.isEmpty()) {
        if (reason) {
            *reason = problem;
        }
        return false;
    }
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From onApplyIfconf on both pages: `ifconf address= netmask= gateway=
    // dhcp=`.
    const quint32 seq = m_wire.writeIfconf(address, netmask, gateway, dhcp);
    Pending pending = pendingOf(Kind::Network);
    pending.dhcp = dhcp;
    pending.address = address;
    pending.netmask = netmask;
    pending.gateway = gateway;
    return sent(seq, pending, reason);
}

bool StationDeviceSettings::saveAndRestart(QString* reason)
{
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From onSaveAndReboot on both pages: `save`.
    return sent(m_wire.save(), pendingOf(Kind::Save), reason);
}

bool StationDeviceSettings::readBack(QString* reason)
{
    if (refuseIfOffline(reason)) {
        return false;
    }
    // From onRevert on both pages: `setup read`, then `ifconf read`.
    if (!sent(m_wire.readSetup(), pendingOf(Kind::ReadSetup), reason)) {
        return false;
    }
    return sent(m_wire.readIfconf(), pendingOf(Kind::ReadNetwork), reason);
}

StationDeviceSettings::Pending StationDeviceSettings::pendingOf(Kind kind, const QString& value)
{
    Pending pending;
    pending.kind = kind;
    pending.value = value;
    return pending;
}

QMap<QString, QString> StationDeviceSettings::fieldsOf(const QString& body)
{
    // key=value words, as the connections parse a reply body.
    QMap<QString, QString> fields;
    for (const QString& part : body.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq > 0) {
            fields.insert(part.left(eq), part.mid(eq + 1));
        }
    }
    return fields;
}

void StationDeviceSettings::onReply(quint32 seq, bool accepted, const QString& body)
{
    const auto it = m_pending.constFind(seq);
    if (it == m_pending.constEnd()) {
        return;
    }
    const Pending pending = it.value();
    m_pending.erase(it);
    AccessorySettingsModel::Device next = current();
    const QString device = deviceName();

    switch (pending.kind) {
    case Kind::Name:
        if (accepted) {
            next.nickname = pending.value;
        }
        answer(&next, accepted ? QStringLiteral("The %1 took the new name.").arg(device)
                               : QStringLiteral("The %1 did not take the new name.").arg(device),
               accepted);
        break;
    case Kind::Bias:
    case Kind::Fan:
    case Kind::Led:
        if (accepted) {
            if (pending.kind == Kind::Bias) {
                next.biasMode = pending.value;
            } else if (pending.kind == Kind::Fan) {
                next.fanMode = pending.value;
            } else {
                next.ledIntensity = pending.number;
            }
        }
        answer(&next,
               accepted ? QStringLiteral("The %1 took the new setting.").arg(device)
                        : QStringLiteral("The %1 did not take the new setting.").arg(device),
               accepted);
        break;
    case Kind::Network:
        if (accepted) {
            next.networkKnown = true;
            next.dhcp = pending.dhcp;
            next.address = pending.address;
            next.netmask = pending.netmask;
            next.gateway = pending.gateway;
        }
        answer(&next,
               accepted
                   ? QStringLiteral("The %1 took the new network settings.").arg(device)
                   : QStringLiteral("The %1 did not take the new network settings.").arg(device),
               accepted);
        break;
    case Kind::Save:
        answer(&next,
               accepted
                   ? QStringLiteral("The %1 is saving its settings and restarting.").arg(device)
                   : QStringLiteral("The %1 did not save its settings.").arg(device),
               accepted);
        break;
    case Kind::ReadSetup: {
        if (accepted) {
            // The fields the local pages read from this answer
            // (onSetupResponse on both pages).
            const QMap<QString, QString> fields = fieldsOf(body);
            if (fields.contains(QStringLiteral("nickname"))) {
                next.nickname = fields.value(QStringLiteral("nickname"));
                // TgxlAdvancedPage::onSetupResponse keeps TGXL_Nickname in
                // step with the tuner.
                if (m_device == Device::Tgxl) {
                    AppSettings::instance().setValue(QStringLiteral("TGXL_Nickname"),
                                                     next.nickname);
                }
            }
            if (m_device == Device::Pgxl) {
                if (fields.contains(QStringLiteral("bias"))) {
                    const QString bias = fields.value(QStringLiteral("bias")).toLower();
                    next.biasMode = bias == QLatin1String("classa")
                                            || bias == QLatin1String("class_a")
                                            || bias == QLatin1String("a")
                                        ? QStringLiteral("ClassA")
                                        : QStringLiteral("ClassAB");
                }
                if (fields.contains(QStringLiteral("fan"))) {
                    const QString fan = fields.value(QStringLiteral("fan"));
                    for (const QString& mode : fanModes()) {
                        if (mode.compare(fan, Qt::CaseInsensitive) == 0) {
                            next.fanMode = mode;
                        }
                    }
                }
                if (fields.contains(QStringLiteral("led"))) {
                    bool ok = false;
                    const int led = fields.value(QStringLiteral("led")).toInt(&ok);
                    if (ok) {
                        next.ledIntensity = led;
                    }
                }
            }
        }
        answer(&next,
               accepted ? QStringLiteral("The %1 sent its settings.").arg(device)
                        : QStringLiteral("The %1 did not send its settings.").arg(device),
               accepted);
        break;
    }
    case Kind::ReadNetwork: {
        if (accepted) {
            // The fields onIfconfResponse reads on both pages.
            const QMap<QString, QString> fields = fieldsOf(body);
            if (fields.contains(QStringLiteral("dhcp"))) {
                const QString dhcp = fields.value(QStringLiteral("dhcp"));
                next.dhcp = dhcp.toLower() == QLatin1String("true") || dhcp == QLatin1String("1");
                next.networkKnown = true;
            }
            if (fields.contains(QStringLiteral("ip"))) {
                next.address = fields.value(QStringLiteral("ip"));
                next.networkKnown = true;
            }
            if (fields.contains(QStringLiteral("netmask"))) {
                next.netmask = fields.value(QStringLiteral("netmask"));
                next.networkKnown = true;
            }
            if (fields.contains(QStringLiteral("gateway"))) {
                next.gateway = fields.value(QStringLiteral("gateway"));
                next.networkKnown = true;
            }
        }
        answer(&next,
               accepted
                   ? QStringLiteral("The %1 sent its network settings.").arg(device)
                   : QStringLiteral("The %1 did not send its network settings.").arg(device),
               accepted);
        break;
    }
    }
    publish(next);
}

void StationDeviceSettings::onDisconnected()
{
    if (m_pending.isEmpty()) {
        return;
    }
    m_pending.clear();
    AccessorySettingsModel::Device next = current();
    answer(&next, QStringLiteral("The %1 went offline before it answered.").arg(deviceName()),
           false);
    publish(next);
}

void StationDeviceSettings::reset()
{
    m_pending.clear();
    // Keep the last answer (a window may still be reading it); forget the
    // values, which belong to the device the Core no longer talks to.
    const AccessorySettingsModel::Device was = current();
    AccessorySettingsModel::Device next;
    next.answer = was.answer;
    next.answerAccepted = was.answerAccepted;
    next.answerCount = was.answerCount;
    publish(next);
}

} // namespace NereusSDR
