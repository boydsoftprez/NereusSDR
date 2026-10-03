// no-port-check: NereusSDR-original accepted-value and persistence model.

#include "PureSignalSettings.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QVariant>
#include <QStringList>

#include <cmath>

namespace NereusSDR {

namespace {

// Fix wave RD-I7: Thetis udPSMoxDelay's range.
// From Thetis PSForm.Designer.cs:346-372 [v2.10.3.15]: DecimalPlaces 1,
// Increment 0.1, Maximum 1.0, Minimum 0.1, Value 0.2.
constexpr double kMoxDelayMinSeconds = 0.1;
constexpr double kMoxDelayMaxSeconds = 1.0;

// Before the fix wave the auto-calibrate preference was saved under a
// camelCase leaf; it is still read so a saved preference survives.
constexpr auto kLegacyAutoCalLeaf = "autoCalEnabled";

QString settingsBool(bool value)
{
    return value ? QStringLiteral("True") : QStringLiteral("False");
}

bool parseStoredBool(const QVariant& stored, bool* value)
{
    const QString text = stored.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
        *value = true;
        return true;
    }
    if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
        *value = false;
        return true;
    }
    return false;
}

} // namespace

bool PureSignalSettingsValues::isValid(QString* reason) const
{
    const auto reject = [reason](const QString& message) {
        if (reason) *reason = message;
        return false;
    };
    if (!std::isfinite(moxDelaySeconds)
        || moxDelaySeconds < kMoxDelayMinSeconds || moxDelaySeconds > kMoxDelayMaxSeconds) {
        return reject(QStringLiteral("MOX delay must be finite and within 0.1 to 1 seconds."));
    }
    if (!std::isfinite(loopDelaySeconds)
        || loopDelaySeconds < 0.0 || loopDelaySeconds > 100.0) {
        return reject(QStringLiteral("Loop delay must be finite and within 0 to 100 seconds."));
    }
    if (!std::isfinite(requestedTxDelayNs)
        || requestedTxDelayNs < 0.0 || requestedTxDelayNs > 25000000.0) {
        return reject(QStringLiteral("Requested TX delay must be finite and within 0 to 25000000 ns."));
    }
    const bool peakValid = std::isfinite(hardwarePeakOverride)
        && hardwarePeakOverride > 0.0;
    if (hardwarePeakOverrideEnabled && !peakValid) {
        return reject(QStringLiteral("An enabled hardware peak override must be a positive finite value."));
    }
    if (hardwarePeakOverride != 0.0 && !peakValid) {
        return reject(QStringLiteral("Hardware peak override must be zero or a positive finite value."));
    }
    if (reason) reason->clear();
    return true;
}

PureSignalSettings::PureSignalSettings(QObject* parent)
    : QObject(parent)
{
}

bool PureSignalSettings::apply(const PureSignalSettingsValues& requested)
{
    QString reason;
    if (!requested.isValid(&reason)) {
        emit editRejected(reason);
        return false;
    }
    if (requested == m_values) return true;
    if (m_editGate) {
        QString gateReason;
        if (!m_editGate(&gateReason)) {
            if (gateReason.isEmpty()) {
                gateReason = QStringLiteral("PureSignal settings cannot be changed from here right now.");
            }
            emit editRejected(gateReason);
            return false;
        }
    }
    const PureSignalSettingsValues before = m_values;
    m_values = requested;
    publishChanges(before, true);
    return true;
}

void PureSignalSettings::initializeAutoCalPreference(bool enabled)
{
    if (m_values.autoCalEnabled == enabled) return;
    const PureSignalSettingsValues before = m_values;
    m_values.autoCalEnabled = enabled;
    publishChanges(before, false);
}

bool PureSignalSettings::applyStationDiagnostic(const QByteArray& propertyName,
                                                const QVariant& value)
{
    if (propertyName != QByteArray("lastLoadError")) return false;
    setLastLoadError(value.toString());
    return true;
}

void PureSignalSettings::setAutoCalEnabled(bool value)
{
    auto requested = m_values;
    requested.autoCalEnabled = value;
    apply(requested);
}

void PureSignalSettings::setRunCalibrationProcessing(bool value)
{
    auto requested = m_values;
    requested.runCalibrationProcessing = value;
    apply(requested);
}

void PureSignalSettings::setAutoAttenuate(bool value)
{
    auto requested = m_values;
    requested.autoAttenuate = value;
    apply(requested);
}

void PureSignalSettings::setQuickAttenuate(bool value)
{
    auto requested = m_values;
    requested.quickAttenuate = value;
    apply(requested);
}

void PureSignalSettings::setMoxDelaySeconds(double value)
{
    auto requested = m_values;
    requested.moxDelaySeconds = value;
    apply(requested);
}

void PureSignalSettings::setLoopDelaySeconds(double value)
{
    auto requested = m_values;
    requested.loopDelaySeconds = value;
    apply(requested);
}

void PureSignalSettings::setRequestedTxDelayNs(double value)
{
    auto requested = m_values;
    requested.requestedTxDelayNs = value;
    apply(requested);
}

void PureSignalSettings::setHardwarePeakOverrideEnabled(bool value)
{
    auto requested = m_values;
    requested.hardwarePeakOverrideEnabled = value;
    apply(requested);
}

void PureSignalSettings::setHardwarePeakOverride(double value)
{
    auto requested = m_values;
    requested.hardwarePeakOverride = value;
    apply(requested);
}

void PureSignalSettings::setRadioIdentity(const QString& mac)
{
    m_radioIdentity = AppSettings::normalizedRadioMac(mac);
}

QString PureSignalSettings::settingsPrefix() const
{
    if (m_radioIdentity.isEmpty()) return {};
    return QStringLiteral("hardware/%1/pureSignal/").arg(m_radioIdentity);
}

bool PureSignalSettings::load(const QString& mac)
{
    setRadioIdentity(mac);
    return load();
}

bool PureSignalSettings::load()
{
    const QString prefix = settingsPrefix();
    if (prefix.isEmpty()) {
        // lastLoadError reaches a remote app as sent: operator words
        // (iPhone app Part A fix wave, R-IOS-01).
        setLastLoadError(QStringLiteral("PureSignal settings could not be loaded because the "
                                        "radio was not identified."));
        return false;
    }

    auto& app = AppSettings::instance();
    PureSignalSettingsValues restored;
    QStringList rejected;

    const auto restoreBool = [&](const QString& leaf, bool& field) {
        const QString key = prefix + leaf;
        if (!app.contains(key)) return;
        bool value = false;
        if (parseStoredBool(app.value(key), &value)) field = value;
        else rejected.append(leaf);
    };
    const auto restoreDouble = [&](const QString& leaf, double& field) {
        const QString key = prefix + leaf;
        if (!app.contains(key)) return;
        bool ok = false;
        const double value = app.value(key).toDouble(&ok);
        auto candidate = restored;
        double PureSignalSettingsValues::* member = nullptr;
        if (leaf == QStringLiteral("MoxDelaySeconds")) member = &PureSignalSettingsValues::moxDelaySeconds;
        else if (leaf == QStringLiteral("LoopDelaySeconds")) member = &PureSignalSettingsValues::loopDelaySeconds;
        else if (leaf == QStringLiteral("RequestedTxDelayNs")) member = &PureSignalSettingsValues::requestedTxDelayNs;
        else member = &PureSignalSettingsValues::hardwarePeakOverride;
        candidate.*member = value;
        if (ok && candidate.isValid()) field = value;
        else rejected.append(leaf);
    };

    if (app.contains(prefix + QStringLiteral("AutoCalEnabled"))) {
        restoreBool(QStringLiteral("AutoCalEnabled"), restored.autoCalEnabled);
    } else {
        restoreBool(QString::fromLatin1(kLegacyAutoCalLeaf), restored.autoCalEnabled);
    }
    restoreBool(QStringLiteral("RunCalibrationProcessing"),
                restored.runCalibrationProcessing);
    restoreBool(QStringLiteral("AutoAttenuate"), restored.autoAttenuate);
    restoreBool(QStringLiteral("QuickAttenuate"), restored.quickAttenuate);
    restoreDouble(QStringLiteral("MoxDelaySeconds"), restored.moxDelaySeconds);
    restoreDouble(QStringLiteral("LoopDelaySeconds"), restored.loopDelaySeconds);
    restoreDouble(QStringLiteral("RequestedTxDelayNs"), restored.requestedTxDelayNs);
    // Restore the value before the enabled flag so a valid enabled override
    // is assessed with its stored positive value.
    restoreDouble(QStringLiteral("HardwarePeakOverride"),
                  restored.hardwarePeakOverride);
    const QString enabledLeaf = QStringLiteral("HardwarePeakOverrideEnabled");
    const QString enabledKey = prefix + enabledLeaf;
    if (app.contains(enabledKey)) {
        bool enabled = false;
        auto candidate = restored;
        if (parseStoredBool(app.value(enabledKey), &enabled)) {
            candidate.hardwarePeakOverrideEnabled = enabled;
            if (candidate.isValid()) restored = candidate;
            else rejected.append(enabledLeaf);
        } else {
            rejected.append(enabledLeaf);
        }
    }

    const PureSignalSettingsValues before = m_values;
    m_values = restored;
    publishChanges(before, false);
    if (!rejected.isEmpty()) {
        qCInfo(lcDsp) << "Saved PureSignal settings not used, defaults applied:" << rejected;
    }
    setLastLoadError(rejected.isEmpty()
        ? QString{}
        : QStringLiteral("Some saved PureSignal settings could not be used, so their defaults "
                         "are in use."));
    return true;
}

bool PureSignalSettings::save() const
{
    const QString prefix = settingsPrefix();
    if (prefix.isEmpty() || !m_values.isValid()) return false;
    auto& app = AppSettings::instance();
    // PascalCase keys and "True"/"False" booleans, as every AppSettings
    // key is (CLAUDE.md, Settings). The old camelCase leaf goes once the
    // new one is written.
    app.setValue(prefix + QStringLiteral("AutoCalEnabled"),
                 settingsBool(m_values.autoCalEnabled));
    app.remove(prefix + QString::fromLatin1(kLegacyAutoCalLeaf));
    app.setValue(prefix + QStringLiteral("RunCalibrationProcessing"),
                 settingsBool(m_values.runCalibrationProcessing));
    app.setValue(prefix + QStringLiteral("AutoAttenuate"),
                 settingsBool(m_values.autoAttenuate));
    app.setValue(prefix + QStringLiteral("QuickAttenuate"),
                 settingsBool(m_values.quickAttenuate));
    app.setValue(prefix + QStringLiteral("MoxDelaySeconds"), m_values.moxDelaySeconds);
    app.setValue(prefix + QStringLiteral("LoopDelaySeconds"), m_values.loopDelaySeconds);
    app.setValue(prefix + QStringLiteral("RequestedTxDelayNs"),
                 m_values.requestedTxDelayNs);
    app.setValue(prefix + QStringLiteral("HardwarePeakOverrideEnabled"),
                 settingsBool(m_values.hardwarePeakOverrideEnabled));
    app.setValue(prefix + QStringLiteral("HardwarePeakOverride"),
                 m_values.hardwarePeakOverride);
    return true;
}

void PureSignalSettings::publishChanges(const PureSignalSettingsValues& before,
                                        bool configurationEdit)
{
    if (before.autoCalEnabled != m_values.autoCalEnabled)
        emit autoCalEnabledChanged(m_values.autoCalEnabled);
    if (before.runCalibrationProcessing != m_values.runCalibrationProcessing)
        emit runCalibrationProcessingChanged(m_values.runCalibrationProcessing);
    if (before.autoAttenuate != m_values.autoAttenuate)
        emit autoAttenuateChanged(m_values.autoAttenuate);
    if (before.quickAttenuate != m_values.quickAttenuate)
        emit quickAttenuateChanged(m_values.quickAttenuate);
    if (before.moxDelaySeconds != m_values.moxDelaySeconds)
        emit moxDelaySecondsChanged(m_values.moxDelaySeconds);
    if (before.loopDelaySeconds != m_values.loopDelaySeconds)
        emit loopDelaySecondsChanged(m_values.loopDelaySeconds);
    if (before.requestedTxDelayNs != m_values.requestedTxDelayNs)
        emit requestedTxDelayNsChanged(m_values.requestedTxDelayNs);
    if (before.hardwarePeakOverrideEnabled != m_values.hardwarePeakOverrideEnabled)
        emit hardwarePeakOverrideEnabledChanged(m_values.hardwarePeakOverrideEnabled);
    if (before.hardwarePeakOverride != m_values.hardwarePeakOverride)
        emit hardwarePeakOverrideChanged(m_values.hardwarePeakOverride);
    if (configurationEdit) emit configurationChanged();
}

void PureSignalSettings::setLastLoadError(const QString& error)
{
    if (error == m_lastLoadError) return;
    m_lastLoadError = error;
    emit lastLoadErrorChanged(error);
}

} // namespace NereusSDR
