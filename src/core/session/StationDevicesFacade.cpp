// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationDevicesFacade.cpp  (NereusSDR)
// =================================================================
// See StationDevicesFacade.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 14 (R-IOS-08): the pairing window's two
//               properties and verbs. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-24: iPhone app Task 17 (R-IOS-08): resetUnclaimed(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I1): the last device is not
//               revoked while no token is active. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I3): a computer enrolled
//               through the token is not revoked while the token works. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): each entry's name numbered
//               by pairing order and its shortName. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: The phone's direct addresses: setCoreAddresses(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 8b: retireTokenAndRevoke and
//               revokeStopsPairingToken. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: Fix wave LINK-I4: pairing opened at the Core
//               (openPairingAtCore) turns pairing through the service
//               back on, and its shut state is shown on the Core. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/StationDevicesFacade.h"

#include "core/AppSettings.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/security/StationLabel.h"
#include "core/security/TokenStore.h"
#include "core/session/DeviceSessionRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <optional>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcDevices, "nereus.station.devices")

QString wireTime(const QDateTime& time)
{
    return time.isValid() ? time.toUTC().toString(Qt::ISODate) : QString();
}

// Sets `key` to `value` and saves; on a failed save the old value is put
// back, so the setting and the file never disagree.
bool storeSetting(AppSettings& settings, const QString& key, const QString& value)
{
    const bool had = settings.contains(key);
    const QVariant previous = settings.value(key);
    settings.setValue(key, value);
    QString error;
    if (settings.save(&error)) {
        return true;
    }
    qCWarning(lcDevices) << "Could not save" << key << ":" << error;
    if (had) {
        settings.setValue(key, previous);
    } else {
        settings.remove(key);
    }
    return false;
}
} // namespace

StationDevicesFacade::StationDevicesFacade(DeviceStore& devices, TokenStore& tokens,
                                           const StationIdentity& identity,
                                           AppSettings& settings, QObject* parent,
                                           PairingWindow* pairingWindow)
    : QObject(parent)
    , m_devices(devices)
    , m_tokens(tokens)
    , m_identity(identity)
    , m_settings(settings)
{
    attachPairingWindow(pairingWindow);
    // The state the object starts from, at revision 0.
    m_state = compute();
    connect(&m_devices, &DeviceStore::devicesChanged, this, &StationDevicesFacade::refresh);
}

StationDevicesFacade::State StationDevicesFacade::compute() const
{
    State state;
    QJsonArray list;
    // iPhone app Task 71 (ruling 4.3): the names and short names numbered as
    // `connectedDevices` numbers them, by pairing order, so one device reads
    // the same on both lists; a missing or unusable short name is the kind's
    // word.
    const QList<PairedDevice> paired = m_devices.list();
    QList<DeviceSessionRegistry::NameInput> order;
    for (const PairedDevice& device : paired) {
        order.append({device.id, device.name,
                      DeviceSessionRegistry::usableShortName(device.shortName, device.kind)});
    }
    const QHash<QByteArray, DeviceSessionRegistry::NumberedName> names =
        DeviceSessionRegistry::numberNames(order);
    for (const PairedDevice& device : paired) {
        const DeviceSessionRegistry::NumberedName name = names.value(device.id);
        list.append(QJsonObject{
            {QStringLiteral("id"), StationIdentity::toBase64Url(device.id)},
            {QStringLiteral("name"), name.name},
            {QStringLiteral("shortName"), name.shortName},
            {QStringLiteral("kind"), device.kind},
            {QStringLiteral("pairedAt"), wireTime(device.pairedAt)},
            {QStringLiteral("lastSeen"), wireTime(device.lastSeen)},
            {QStringLiteral("connected"), m_connected.contains(device.id)},
        });
    }
    state.listJson = QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact));
    const std::optional<StationLabel> label = StationLabel::current(m_settings);
    state.stationLabel = label ? label->display() : QString();
    state.claimed = m_devices.isClaimed();
    state.tokenActive = m_tokens.isActive();
    if (m_identity.isValid()) {
        const QString acknowledged =
            m_settings.value(QLatin1String(kKeyBackupSettingsKey), QString()).toString();
        state.keyBackupAcknowledged =
            acknowledged == StationIdentity::toBase64Url(m_identity.fingerprint());
        state.keyPath = m_identity.keyPath();
    }
    if (m_pairingWindow != nullptr) {
        state.pairingWindowOpen = m_pairingWindow->isOpen();
        state.pairingCode = m_pairingWindow->currentCode();
        state.servicePairingShut = m_pairingWindow->isServiceShut();
    }
    return state;
}

void StationDevicesFacade::attachPairingWindow(PairingWindow* window)
{
    if (m_pairingWindow != nullptr) {
        disconnect(m_pairingWindow, nullptr, this, nullptr);
    }
    m_pairingWindow = window;
    if (m_pairingWindow != nullptr) {
        connect(m_pairingWindow, &PairingWindow::stateChanged, this,
                &StationDevicesFacade::refresh);
        connect(m_pairingWindow, &PairingWindow::codeChanged, this,
                &StationDevicesFacade::refresh);
        connect(m_pairingWindow, &PairingWindow::serviceShutChanged, this,
                &StationDevicesFacade::refresh);
    }
}

void StationDevicesFacade::setPairingWindow(PairingWindow* window)
{
    if (m_pairingWindow == window) {
        return;
    }
    attachPairingWindow(window);
    refresh();
}

void StationDevicesFacade::refresh()
{
    if (m_hold > 0) {
        m_refreshWanted = true;
        return;
    }
    const State next = compute();
    if (next == m_state) {
        return;
    }
    const bool labelChanged = next.stationLabel != m_state.stationLabel;
    m_state = next;
    ++m_revision;  // wraps past 2^32; readers compare by serial number
    emit devicesStateChanged();
    if (labelChanged) {
        emit stationLabelChanged(m_state.stationLabel);
    }
}

void StationDevicesFacade::holdRefresh()
{
    ++m_hold;
}

void StationDevicesFacade::resumeRefresh()
{
    if (m_hold > 0) {
        --m_hold;
    }
    if (m_hold == 0 && m_refreshWanted) {
        m_refreshWanted = false;
        refresh();
    }
}

void StationDevicesFacade::setConnectedDevices(const QSet<QByteArray>& ids)
{
    if (ids == m_connected) {
        return;
    }
    m_connected = ids;
    refresh();
}

void StationDevicesFacade::setCoreAddresses(const QString& json)
{
    if (json == m_coreAddresses) {
        return;
    }
    m_coreAddresses = json;
    emit coreAddressesChanged();
}

DeviceAdminResult StationDevicesFacade::revoke(const QString& id)
{
    if (!m_devices.isValid()) {
        return {false, QStringLiteral("The Core cannot read its list of paired devices, so it "
                                      "cannot remove one.")};
    }
    bool ok = false;
    const QByteArray raw = StationIdentity::fromBase64Url(id, &ok);
    const std::optional<PairedDevice> device = ok ? m_devices.find(raw) : std::nullopt;
    if (!device) {
        return {false, QStringLiteral("That device is not paired with this Core.")};
    }
    // Fix wave R1-I1: the last device of a Core with no token is what keeps
    // it claimed. Removing it would open the pairing window to one tap on
    // the Core's network and to codes from anywhere; the pairing design
    // closes the window for good at the first pair, and only physical
    // access (the console's reset) makes a Core unclaimed again. The same
    // guard as retireToken's, from the other side.
    if (!m_tokens.isActive() && m_devices.list().size() <= 1) {
        return {false, QStringLiteral("Pair another device first, or reset this Core from its "
                                      "own computer.")};
    }
    // Fix wave R1-I3: a computer that enrolled through the token would be
    // enrolled again at its next token sign-in, so removing it while the
    // token works keeps no one out.
    if (device->enrolledThroughToken && m_tokens.isActive()) {
        return {false, QStringLiteral("Stop accepting the pairing token first, then remove this "
                                      "computer.")};
    }
    // DeviceStore emits deviceRemoved (StationServer ends that device's
    // connection) and devicesChanged (refresh) on success.
    if (!m_devices.remove(raw)) {
        return {false, QStringLiteral("The Core could not remove that device. Try again.")};
    }
    qCInfo(lcDevices) << "A paired device was removed";
    return {true, QString()};
}

bool StationDevicesFacade::revokeStopsPairingToken(const QString& id) const
{
    if (!m_devices.isValid() || !m_tokens.isActive()) {
        return false;
    }
    bool ok = false;
    const QByteArray raw = StationIdentity::fromBase64Url(id, &ok);
    const std::optional<PairedDevice> device = ok ? m_devices.find(raw) : std::nullopt;
    return device && device->enrolledThroughToken;
}

DeviceAdminResult StationDevicesFacade::retireTokenAndRevoke(const QString& id)
{
    // Slice control plan Task 8b (JJ's bench: a computer that joined with
    // the token could not be removed from This Core). Fix wave R1-I3 still
    // holds: while the token works such a computer would join again, so the
    // token stops first. Anything else is revoke() as it is.
    if (!revokeStopsPairingToken(id)) {
        return revoke(id);
    }
    // Checked before anything changes: once the token stops, the removal
    // must not leave the Core with no paired device (Fix wave R1-I1), which
    // revoke() would refuse after the token was already gone.
    if (m_devices.list().size() <= 1) {
        return {false, QStringLiteral("Pair another device first, or reset this Core from its "
                                      "own computer.")};
    }
    const DeviceAdminResult retired = retireToken();
    if (!retired.accepted) {
        return retired;
    }
    return revoke(id);
}

DeviceAdminResult StationDevicesFacade::rename(const QString& label)
{
    const std::optional<StationLabel> parsed = StationLabel::parse(label);
    if (!parsed) {
        return {false, StationLabel::ruleText()};
    }
    if (!storeSetting(m_settings, QLatin1String(StationLabel::kSettingsKey),
                      parsed->display())) {
        return {false, QStringLiteral("The Core could not save its new name. Try again.")};
    }
    refresh();
    return {true, QString()};
}

DeviceAdminResult StationDevicesFacade::acknowledgeKeyBackup()
{
    if (!m_identity.isValid()) {
        return {false, QStringLiteral("The Core's own key is unavailable, so there is nothing "
                                      "to back up.")};
    }
    if (!storeSetting(m_settings, QLatin1String(kKeyBackupSettingsKey),
                      StationIdentity::toBase64Url(m_identity.fingerprint()))) {
        return {false, QStringLiteral("The Core could not save this. Try again.")};
    }
    refresh();
    return {true, QString()};
}

DeviceAdminResult StationDevicesFacade::retireToken()
{
    // So the owner cannot lock everyone out: the token stays until a
    // device can sign in with its own key.
    if (!m_devices.isValid() || m_devices.list().isEmpty()) {
        return {false, QStringLiteral("Pair a device with this Core first, so a device can "
                                      "still sign in once the pairing token stops working.")};
    }
    if (!m_tokens.isActive()) {
        // Nothing to retire; a second request changes nothing.
        return {true, QString()};
    }
    if (!m_tokens.retire()) {
        return {false, QStringLiteral("The Core could not stop accepting its pairing token. "
                                      "Try again.")};
    }
    qCInfo(lcDevices) << "The pairing token was retired";
    refresh();
    emit tokenRetired();
    return {true, QString()};
}

DeviceAdminResult StationDevicesFacade::openPairing()
{
    if (m_pairingWindow == nullptr) {
        return {false, QStringLiteral("This Core cannot pair new devices.")};
    }
    // The window's own signals refresh the object.
    m_pairingWindow->reopen();
    return {true, QString()};
}

DeviceAdminResult StationDevicesFacade::openPairingAtCore()
{
    if (m_pairingWindow == nullptr) {
        return {false, QStringLiteral("This Core cannot pair new devices.")};
    }
    m_pairingWindow->reopenAtCore();
    return {true, QString()};
}

DeviceAdminResult StationDevicesFacade::closePairing()
{
    if (m_pairingWindow == nullptr) {
        return {false, QStringLiteral("This Core cannot pair new devices.")};
    }
    m_pairingWindow->close();
    return {true, QString()};
}

DeviceAdminResult StationDevicesFacade::resetUnclaimed()
{
    // One change for the object, however many steps below move it.
    holdRefresh();
    // The token first: the device list's change below is what the pairing
    // window follows, and it must find the Core unclaimed by then.
    const bool tokenWasActive = m_tokens.isActive() || !m_tokens.isValid();
    if (!m_tokens.moveDamagedAside() || !m_tokens.retire()) {
        resumeRefresh();
        return {false, QStringLiteral("The Core could not stop accepting its pairing token, so "
                                      "nothing was reset. Try again.")};
    }
    if (tokenWasActive) {
        emit tokenRetired();
    }
    if (!m_devices.reset()) {
        resumeRefresh();
        return {false, QStringLiteral("The Core could not clear its list of paired devices. "
                                      "Try again.")};
    }
    qCInfo(lcDevices) << "The Core was reset to unclaimed from its console";
    // The device list's change opens a claimed Core's window; one the
    // attempt ceiling had closed (already unclaimed) opens here, afresh.
    if (m_pairingWindow != nullptr) {
        // LINK-I4: the console's reset is a reopening at the Core.
        m_pairingWindow->reopenAtCore();
    }
    refresh();
    resumeRefresh();
    return {true, QString()};
}

} // namespace NereusSDR
