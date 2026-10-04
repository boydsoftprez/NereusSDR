// no-port-check: NereusSDR-original accepted-value model for normal PS3
// configuration. DSP algorithms and operational commands are not stored here.

#pragma once

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVariant>

#include <functional>
#include <utility>

namespace NereusSDR {

struct PureSignalSettingsValues {
    bool autoCalEnabled{false};
    bool runCalibrationProcessing{true};
    bool autoAttenuate{true};
    bool quickAttenuate{false};
    // Fix wave RD-I7: Thetis udPSMoxDelay, Value 0.2, Minimum 0.1,
    // Maximum 1.0. From Thetis PSForm.Designer.cs:346-372 [v2.10.3.15].
    double moxDelaySeconds{0.2};
    double loopDelaySeconds{0.0};
    double requestedTxDelayNs{150.0};
    bool hardwarePeakOverrideEnabled{false};
    double hardwarePeakOverride{0.0};

    bool isValid(QString* reason = nullptr) const;
    bool operator==(const PureSignalSettingsValues&) const = default;
};

class NEREUS_CORE_EXPORT PureSignalSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autoCalEnabled READ autoCalEnabled WRITE setAutoCalEnabled
               NOTIFY autoCalEnabledChanged)
    Q_PROPERTY(bool runCalibrationProcessing READ runCalibrationProcessing
               WRITE setRunCalibrationProcessing
               NOTIFY runCalibrationProcessingChanged)
    Q_PROPERTY(bool autoAttenuate READ autoAttenuate WRITE setAutoAttenuate
               NOTIFY autoAttenuateChanged)
    Q_PROPERTY(bool quickAttenuate READ quickAttenuate WRITE setQuickAttenuate
               NOTIFY quickAttenuateChanged)
    Q_PROPERTY(double moxDelaySeconds READ moxDelaySeconds WRITE setMoxDelaySeconds
               NOTIFY moxDelaySecondsChanged)
    Q_PROPERTY(double loopDelaySeconds READ loopDelaySeconds WRITE setLoopDelaySeconds
               NOTIFY loopDelaySecondsChanged)
    Q_PROPERTY(double requestedTxDelayNs READ requestedTxDelayNs WRITE setRequestedTxDelayNs
               NOTIFY requestedTxDelayNsChanged)
    Q_PROPERTY(bool hardwarePeakOverrideEnabled READ hardwarePeakOverrideEnabled
               WRITE setHardwarePeakOverrideEnabled
               NOTIFY hardwarePeakOverrideEnabledChanged)
    Q_PROPERTY(double hardwarePeakOverride READ hardwarePeakOverride
               WRITE setHardwarePeakOverride NOTIFY hardwarePeakOverrideChanged)
    Q_PROPERTY(QString lastLoadError READ lastLoadError NOTIFY lastLoadErrorChanged)

public:
    using EditGate = std::function<bool(QString* reason)>;

    explicit PureSignalSettings(QObject* parent = nullptr);

    PureSignalSettingsValues values() const noexcept { return m_values; }
    bool apply(const PureSignalSettingsValues& requested);
    void initializeAutoCalPreference(bool enabled);
    void setEditGate(EditGate gate) { m_editGate = std::move(gate); }
    bool applyStationDiagnostic(const QByteArray& propertyName,
                                const QVariant& value);

    bool autoCalEnabled() const noexcept { return m_values.autoCalEnabled; }
    bool runCalibrationProcessing() const noexcept { return m_values.runCalibrationProcessing; }
    bool autoAttenuate() const noexcept { return m_values.autoAttenuate; }
    bool quickAttenuate() const noexcept { return m_values.quickAttenuate; }
    double moxDelaySeconds() const noexcept { return m_values.moxDelaySeconds; }
    double loopDelaySeconds() const noexcept { return m_values.loopDelaySeconds; }
    double requestedTxDelayNs() const noexcept { return m_values.requestedTxDelayNs; }
    bool hardwarePeakOverrideEnabled() const noexcept {
        return m_values.hardwarePeakOverrideEnabled;
    }
    double hardwarePeakOverride() const noexcept { return m_values.hardwarePeakOverride; }

    void setAutoCalEnabled(bool value);
    void setRunCalibrationProcessing(bool value);
    void setAutoAttenuate(bool value);
    void setQuickAttenuate(bool value);
    void setMoxDelaySeconds(double value);
    void setLoopDelaySeconds(double value);
    void setRequestedTxDelayNs(double value);
    void setHardwarePeakOverrideEnabled(bool value);
    void setHardwarePeakOverride(double value);

    void setRadioIdentity(const QString& mac);
    QString radioIdentity() const { return m_radioIdentity; }
    QString settingsPrefix() const;
    bool load(const QString& mac);
    bool load();
    bool save() const;

    QString lastLoadError() const { return m_lastLoadError; }

signals:
    void autoCalEnabledChanged(bool);
    void runCalibrationProcessingChanged(bool);
    void autoAttenuateChanged(bool);
    void quickAttenuateChanged(bool);
    void moxDelaySecondsChanged(double);
    void loopDelaySecondsChanged(double);
    void requestedTxDelayNsChanged(double);
    void hardwarePeakOverrideEnabledChanged(bool);
    void hardwarePeakOverrideChanged(double);
    void configurationChanged();
    void editRejected(const QString& reason);
    void lastLoadErrorChanged(const QString& error);

private:
    void publishChanges(const PureSignalSettingsValues& before,
                        bool configurationEdit);
    void setLastLoadError(const QString& error);

    PureSignalSettingsValues m_values;
    QString m_radioIdentity;
    QString m_lastLoadError;
    EditGate m_editGate;
};

} // namespace NereusSDR
