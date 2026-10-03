// no-port-check: NereusSDR-original.
// =================================================================
// src/core/daemon/StationControlCommands.cpp  (NereusSDR)
// =================================================================
// See StationControlCommands.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: the command list says a packaged Core needs only sudo
//               (R-IOS-08, R-R3-26). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: Fix wave LINK-I4: pairing opened at the Core
//               (openPairingAtCore) turns pairing through the service
//               back on, and its shut state is shown on the Core. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/daemon/StationControlCommands.h"

#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationServer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace NereusSDR {

namespace {

const QString kRemoteOff = QStringLiteral(
    "Remote access is off on this Core, so it has no paired devices and no pairing. "
    "Set remote_port in its configuration file to turn remote access on.");
const QString kCannotPair = QStringLiteral("This Core cannot pair new devices.");
const QString kUnreadableList = QStringLiteral(
    "This Core cannot read its list of paired devices. nereusd reset --unclaimed --yes "
    "moves the damaged list aside and starts again with no paired devices.");

QString radioText(const StationRadioStatus& radio)
{
    if (!radio.connected) {
        return QStringLiteral("off");
    }
    QStringList parts;
    if (!radio.model.isEmpty()) {
        parts << radio.model;
    }
    if (!radio.name.isEmpty() && radio.name != radio.model) {
        parts << radio.name;
    }
    parts << QStringLiteral("connected");
    return parts.join(QStringLiteral(", "));
}

QString shownTime(const QString& iso)
{
    const QDateTime time = QDateTime::fromString(iso, Qt::ISODate);
    return time.isValid() ? time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                          : QStringLiteral("never");
}

QJsonArray deviceList(const StationServer& server)
{
    const StationDevicesFacade* devices = server.devicesFacade();
    return devices ? QJsonDocument::fromJson(devices->listJson().toUtf8()).array() : QJsonArray{};
}

} // namespace

StationControlCommands::StationControlCommands(Sources sources)
    : m_sources(std::move(sources))
{
}

bool StationControlCommands::isCommand(const QString& word)
{
    static const QStringList words{QStringLiteral("status"), QStringLiteral("pairing"),
                                   QStringLiteral("devices"), QStringLiteral("token"),
                                   QStringLiteral("reset"), QStringLiteral("release")};
    return words.contains(word);
}

QString StationControlCommands::usage()
{
    return QStringLiteral(
        "The commands are:\n"
        "  nereusd status\n"
        "  nereusd pairing show\n"
        "  nereusd pairing open\n"
        "  nereusd pairing close\n"
        "  nereusd devices\n"
        "  nereusd devices revoke <id>\n"
        "  nereusd token retire\n"
        "  nereusd reset --unclaimed --yes\n"
        "  nereusd release\n"
        "On a packaged Core, run each with sudo and nothing else, for example sudo nereusd "
        "status. For a Core you started yourself, run each from the same account with the "
        "same --config and --profile it was started with.");
}

StationServer* StationControlCommands::server() const
{
    return m_sources.server ? m_sources.server() : nullptr;
}

StationControlReply StationControlCommands::execute(const QStringList& args) const
{
    QStringList words;
    bool unclaimed = false;
    bool yes = false;
    for (const QString& arg : args) {
        if (arg == QLatin1String("--unclaimed")) {
            unclaimed = true;
        } else if (arg == QLatin1String("--yes")) {
            yes = true;
        } else {
            words << arg;
        }
    }
    const QString first = words.value(0);
    const QString second = words.value(1);
    if (first == QLatin1String("status") && words.size() == 1) {
        return status();
    }
    if (first == QLatin1String("pairing") && words.size() == 2) {
        if (second == QLatin1String("show")) {
            return pairingShow();
        }
        if (second == QLatin1String("open")) {
            return pairingOpen();
        }
        if (second == QLatin1String("close")) {
            return pairingClose();
        }
    }
    if (first == QLatin1String("devices") && words.size() == 1) {
        return devices();
    }
    if (first == QLatin1String("devices") && second == QLatin1String("revoke")
        && words.size() == 3) {
        return revoke(words.at(2));
    }
    if (first == QLatin1String("token") && second == QLatin1String("retire")
        && words.size() == 2) {
        return retireToken();
    }
    if (first == QLatin1String("reset") && words.size() == 1) {
        return reset(unclaimed, yes);
    }
    if (first == QLatin1String("release") && words.size() == 1) {
        return {false, QStringLiteral("This Core cannot hand back the radio through this "
                                      "control socket.")};
    }
    return {false, QStringLiteral("Unknown command.\n") + usage()};
}

StationControlReply StationControlCommands::status() const
{
    const QString label = m_sources.label ? m_sources.label() : QString();
    const StationRadioStatus radio = m_sources.radio ? m_sources.radio() : StationRadioStatus{};
    const QString page = m_sources.statusPageAddress ? m_sources.statusPageAddress() : QString();
    QStringList lines;
    lines << QStringLiteral("This Core: %1").arg(label);
    lines << QStringLiteral("Radio: %1").arg(radioText(radio));
    StationServer* core = server();
    if (core == nullptr) {
        lines << QStringLiteral("Remote access: off");
    } else {
        lines << QStringLiteral("Remote access: on");
        if (!core->deviceStore()->isValid()) {
            lines << QStringLiteral("Paired devices: this Core cannot read its list");
        } else {
            lines << QStringLiteral("Paired devices: %1").arg(core->deviceStore()->list().size());
        }
        const PairingWindow* window = core->pairingWindow();
        QString pairing = QStringLiteral("closed");
        if (core->pairingVersion() < 1 || window == nullptr) {
            pairing = QStringLiteral("not available on this Core");
        } else if (window->state() == PairingWindow::State::OpenUnclaimed) {
            pairing = QStringLiteral("open until the first device pairs "
                                     "(nereusd pairing show gives the code)");
        } else if (window->state() == PairingWindow::State::OpenReopened) {
            pairing = QStringLiteral("open for one more device "
                                     "(nereusd pairing show gives the code)");
        } else if (window->state() == PairingWindow::State::ClosedUnclaimed) {
            pairing = QStringLiteral("closed after too many wrong pairing codes "
                                     "(nereusd pairing open opens it again)");
        }
        lines << QStringLiteral("Pairing: %1").arg(pairing);
    }
    lines << QStringLiteral("Status page: %1")
                 .arg(page.isEmpty() ? QStringLiteral("off") : page);
    return {true, lines.join(QLatin1Char('\n'))};
}

StationControlReply StationControlCommands::pairingShow(const QString& lead) const
{
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    const PairingWindow* window = core->pairingWindow();
    if (core->pairingVersion() < 1 || window == nullptr) {
        return {false, kCannotPair};
    }
    QStringList lines;
    if (!lead.isEmpty()) {
        lines << lead;
    }
    switch (window->state()) {
    case PairingWindow::State::ClosedClaimed:
        lines << QStringLiteral("Pairing is closed. nereusd pairing open lets one more device "
                                "pair.");
        return {true, lines.join(QLatin1Char('\n'))};
    case PairingWindow::State::ClosedUnclaimed:
        lines << QStringLiteral("Pairing closed after too many wrong pairing codes. nereusd "
                                "pairing open opens it again.");
        return {true, lines.join(QLatin1Char('\n'))};
    case PairingWindow::State::OpenUnclaimed:
        lines << QStringLiteral("Pairing is open: no device is paired with this Core.");
        break;
    case PairingWindow::State::OpenReopened:
        lines << QStringLiteral("Pairing is open for one more device.");
        break;
    }
    if (window->isServiceShut()) {
        // LINK-I4.
        lines << QStringLiteral("Pairing from outside your network is off after too many wrong "
                                "codes. Run nereusd pairing open to turn it back on.");
    }
    const QString code = window->currentCode();
    if (!code.isEmpty()) {
        lines << QStringLiteral("Pairing code: %1").arg(code);
        lines << QStringLiteral("Type it into the NereusSDR app to pair a device with this Core.");
    } else if (window->codeInUse()) {
        lines << QStringLiteral("A device is pairing now. Run this again in a few seconds.");
    } else {
        const qint64 seconds = (window->retryAfterMs() + 999) / 1000;
        lines << QStringLiteral("The next pairing code appears in %1 seconds.").arg(seconds);
    }
    return {true, lines.join(QLatin1Char('\n'))};
}

StationControlReply StationControlCommands::pairingOpen() const
{
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    PairingWindow* window = core->pairingWindow();
    if (core->pairingVersion() < 1 || window == nullptr) {
        return {false, kCannotPair};
    }
    if (window->state() == PairingWindow::State::OpenUnclaimed) {
        // LINK-I4: opening it here still turns pairing from outside the
        // network back on.
        core->devicesFacade()->openPairingAtCore();
        return pairingShow(QStringLiteral("Pairing is already open, since no device has paired "
                                          "with this Core."));
    }
    if (!core->deviceStore()->isValid()) {
        return {false, kUnreadableList};
    }
    const DeviceAdminResult result = core->devicesFacade()->openPairingAtCore();
    if (!result.accepted) {
        return {false, result.reason};
    }
    return pairingShow();
}

StationControlReply StationControlCommands::pairingClose() const
{
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    PairingWindow* window = core->pairingWindow();
    if (core->pairingVersion() < 1 || window == nullptr) {
        return {false, kCannotPair};
    }
    switch (window->state()) {
    case PairingWindow::State::OpenUnclaimed:
        return {false, QStringLiteral("Pairing stays open until the first device pairs with "
                                      "this Core.")};
    case PairingWindow::State::ClosedClaimed:
    case PairingWindow::State::ClosedUnclaimed:
        return {true, QStringLiteral("Pairing is already closed.")};
    case PairingWindow::State::OpenReopened:
        break;
    }
    const DeviceAdminResult result = core->devicesFacade()->closePairing();
    if (!result.accepted) {
        return {false, result.reason};
    }
    return {true, QStringLiteral("Pairing is closed.")};
}

StationControlReply StationControlCommands::devices() const
{
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    if (!core->deviceStore()->isValid()) {
        return {false, kUnreadableList};
    }
    const QJsonArray list = deviceList(*core);
    if (list.isEmpty()) {
        return {true, QStringLiteral("No device has paired with this Core.")};
    }
    QStringList lines;
    for (const QJsonValue& value : list) {
        const QJsonObject device = value.toObject();
        lines << QStringLiteral("%1 (%2), paired %3, last seen %4%5")
                     .arg(device.value(QStringLiteral("name")).toString(),
                          device.value(QStringLiteral("kind")).toString(),
                          shownTime(device.value(QStringLiteral("pairedAt")).toString()),
                          shownTime(device.value(QStringLiteral("lastSeen")).toString()),
                          device.value(QStringLiteral("connected")).toBool()
                              ? QStringLiteral(", connected now")
                              : QString());
        lines << QStringLiteral("  id %1").arg(device.value(QStringLiteral("id")).toString());
    }
    lines << QStringLiteral("To remove one: nereusd devices revoke <id>");
    return {true, lines.join(QLatin1Char('\n'))};
}

StationControlReply StationControlCommands::revoke(const QString& id) const
{
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    QString name;
    for (const QJsonValue& value : deviceList(*core)) {
        if (value.toObject().value(QStringLiteral("id")).toString() == id) {
            name = value.toObject().value(QStringLiteral("name")).toString();
        }
    }
    const DeviceAdminResult result = core->devicesFacade()->revoke(id);
    if (!result.accepted) {
        return {false, result.reason};
    }
    return {true, QStringLiteral("%1 was removed from this Core and can no longer connect.")
                      .arg(name.isEmpty() ? QStringLiteral("The device") : name)};
}

StationControlReply StationControlCommands::retireToken() const
{
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    StationDevicesFacade* facade = core->devicesFacade();
    const bool active = facade->tokenActive();
    const DeviceAdminResult result = facade->retireToken();
    if (!result.accepted) {
        return {false, result.reason};
    }
    if (!active) {
        return {true, QStringLiteral("This Core has no pairing token to retire.")};
    }
    return {true, QStringLiteral("The pairing token no longer works. Anything that signed in "
                                 "with it was disconnected; paired devices sign in with their "
                                 "own keys.")};
}

StationControlReply StationControlCommands::reset(bool unclaimed, bool yes) const
{
    if (!unclaimed) {
        return {false, QStringLiteral("Use nereusd reset --unclaimed --yes to remove every paired "
                                      "device and open pairing again.")};
    }
    StationServer* core = server();
    if (core == nullptr) {
        return {false, kRemoteOff};
    }
    if (!yes) {
        return {false, QStringLiteral(
                           "This removes every paired device, stops the pairing token working, "
                           "disconnects everything connected to this Core, and opens pairing "
                           "to the next device. Nothing was changed. To go ahead, run "
                           "nereusd reset --unclaimed --yes")};
    }
    const DeviceAdminResult result = core->devicesFacade()->resetUnclaimed();
    if (!result.accepted) {
        return {false, result.reason};
    }
    return pairingShow(QStringLiteral("This Core is reset: no device is paired with it."));
}

} // namespace NereusSDR
