#pragma once
// no-port-check: NereusSDR-original mirrored presentation of the Core's step
// attenuator and preamp. All attenuator logic stays in StepAttenuatorController.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/StepAttenuatorFacade.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. The Core's step attenuator and
// preamp as one mirrored object, `stepAtt` (R-R3-46, R-R3-11, R-R3-13).
//
// Bound to a StepAttenuatorController (the Core, and a local window) it
// follows every controller change and applies each edit through the
// controller, so the radio, the per-band memory and the saved settings stay
// the controller's. A property the controller cannot take as asked settles
// on the value it kept, and settleReason() says why in plain words.
//
// Unbound (a remote window) it only holds values: the Core's, as they
// arrive, and the window's edits, which an edit gate may refuse.
//
// Wire values: preampMode is the PreampMode integer (0 Off .. 6 -50 dB);
// autoAttMode is 0 Classic, 1 Adaptive; overloadAdc0/1 are 0 none,
// 1 yellow, 2 red; both auto-attenuate times are whole seconds in ms.
// rx2StepAttEnabled, rx2AutoAttEnabled, rx2AutoAttUndo and
// rx2AutoAttUndoDelayMs are RX2's own enable and auto-attenuate settings.
// rx2PreampMode is RX2's own preamp mode (a PreampMode integer, the items
// rx2PreampItemsForBoard offers), the one the slices on the other ADC use.
// rx2AttenuationDb is the attenuator of the ADC slice A is not on (Thetis
// RX2's); rx2SliceMask has bit n set for each slice n on that ADC, which
// reads and sets rx2AttenuationDb rather than attenuationDb (0: every slice
// uses attenuationDb, one ADC in use or diversity linking both).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  A remote window's availability (can
//                                    its edits reach the Core, and if not
//                                    why) for the RX applet and Setup
//                                    (R-R3-46, R-R3-21). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 5): Setup >
//                                    Transmit > Power's ATT on TX, its
//                                    value and Force ATT (attOnTxEnabled,
//                                    attOnTxValue, forceAttWhenPsOff),
//                                    transmitSettingsVersion 5.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the other ADC's own
//                                    attenuator and the slices on it
//                                    (rx2AttenuationDb, rx2SliceMask),
//                                    adcAttenuatorVersion 1. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: rx2StepAttEnabled,
//                                    rx2AutoAttEnabled, rx2AutoAttUndo,
//                                    rx2AutoAttUndoDelayMs. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal: rx2PreampMode, RX2's own
//                                    preamp mode (radioHardwareVersion 12).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <functional>

namespace NereusSDR {

class RadioModel;
class StepAttenuatorController;

class StepAttenuatorFacade final : public QObject {
    Q_OBJECT
    // Settable.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int attenuationDb READ attenuationDb WRITE setAttenuationDb
               NOTIFY attenuationDbChanged)
    Q_PROPERTY(int preampMode READ preampMode WRITE setPreampMode NOTIFY preampModeChanged)
    Q_PROPERTY(bool rx1Preamp READ rx1Preamp WRITE setRx1Preamp NOTIFY rx1PreampChanged)
    Q_PROPERTY(bool autoAttEnabled READ autoAttEnabled WRITE setAutoAttEnabled
               NOTIFY autoAttEnabledChanged)
    Q_PROPERTY(int autoAttMode READ autoAttMode WRITE setAutoAttMode NOTIFY autoAttModeChanged)
    Q_PROPERTY(bool autoAttUndo READ autoAttUndo WRITE setAutoAttUndo NOTIFY autoAttUndoChanged)
    Q_PROPERTY(int autoAttUndoDelayMs READ autoAttUndoDelayMs WRITE setAutoAttUndoDelayMs
               NOTIFY autoAttUndoDelayMsChanged)
    Q_PROPERTY(int autoAttHoldMs READ autoAttHoldMs WRITE setAutoAttHoldMs
               NOTIFY autoAttHoldMsChanged)
    // Reported by the Core.
    Q_PROPERTY(int minDb READ minDb NOTIFY minDbChanged)
    Q_PROPERTY(int maxDb READ maxDb NOTIFY maxDbChanged)
    Q_PROPERTY(bool autoAttApplied READ autoAttApplied NOTIFY autoAttAppliedChanged)
    Q_PROPERTY(int overloadAdc0 READ overloadAdc0 NOTIFY overloadAdc0Changed)
    Q_PROPERTY(int overloadAdc1 READ overloadAdc1 NOTIFY overloadAdc1Changed)
    Q_PROPERTY(bool adcLinked READ adcLinked NOTIFY adcLinkedChanged)
    // R-R3-49 (parity Task 5, transmitSettingsVersion 5): Setup > Transmit >
    // Power's ATT on TX, its value in dB (the current transmit band's) and
    // Force ATT on TX to 31 when PS-A is off. Transmit settings: a
    // receive-only Core takes them only while its radio is off the air.
    // Declared last so the earlier ordinals stay put.
    Q_PROPERTY(bool attOnTxEnabled READ attOnTxEnabled WRITE setAttOnTxEnabled
               NOTIFY attOnTxEnabledChanged)
    Q_PROPERTY(int attOnTxValue READ attOnTxValue WRITE setAttOnTxValue
               NOTIFY attOnTxValueChanged)
    Q_PROPERTY(bool forceAttWhenPsOff READ forceAttWhenPsOff WRITE setForceAttWhenPsOff
               NOTIFY forceAttWhenPsOffChanged)
    // R-R3-46 / R-R3-11 (adcAttenuatorVersion 1): the attenuator of the ADC
    // slice A is not on (Thetis RX2's) and the slices on that ADC. Sent only
    // to a peer whose hello declared adcAttenuators 1. Declared last so the
    // earlier ordinals stay put.
    Q_PROPERTY(int rx2AttenuationDb READ rx2AttenuationDb WRITE setRx2AttenuationDb
               NOTIFY rx2AttenuationDbChanged)
    Q_PROPERTY(int rx2SliceMask READ rx2SliceMask NOTIFY rx2SliceMaskChanged)
    // RX2's own step attenuator enable and auto-attenuate settings (Thetis
    // _rx2_step_att_enabled, _auto_att_rx2, _auto_att_undo_rx2,
    // _auto_att_hold_delay_rx2; the delay in whole seconds, carried in ms).
    Q_PROPERTY(bool rx2StepAttEnabled READ rx2StepAttEnabled WRITE setRx2StepAttEnabled
               NOTIFY rx2StepAttEnabledChanged)
    Q_PROPERTY(bool rx2AutoAttEnabled READ rx2AutoAttEnabled WRITE setRx2AutoAttEnabled
               NOTIFY rx2AutoAttEnabledChanged)
    Q_PROPERTY(bool rx2AutoAttUndo READ rx2AutoAttUndo WRITE setRx2AutoAttUndo
               NOTIFY rx2AutoAttUndoChanged)
    Q_PROPERTY(int rx2AutoAttUndoDelayMs READ rx2AutoAttUndoDelayMs
               WRITE setRx2AutoAttUndoDelayMs NOTIFY rx2AutoAttUndoDelayMsChanged)
    // Level Cal (radioHardwareVersion 12): RX2's own preamp mode (Thetis
    // RX2PreampMode). Appended last so earlier ordinals stay put.
    Q_PROPERTY(int rx2PreampMode READ rx2PreampMode WRITE setRx2PreampMode
               NOTIFY rx2PreampModeChanged)

public:
    /// True when an edit may go ahead; otherwise false with a plain reason.
    using EditGate = std::function<bool(QString* reason)>;

    /// Whole seconds, as the Setup spinboxes and the saved settings hold
    /// them (GeneralOptionsPage: 1..3600 s).
    static constexpr int kMinAutoAttTimeMs = 1000;
    static constexpr int kMaxAutoAttTimeMs = 3600 * 1000;
    /// R-R3-49 (parity Task 5): the top of the ATT on TX value, from
    /// StepAttenuatorController::setAttOnTxValue (Thetis setup.cs:3999
    /// clamps above 31). The bottom is minDb, the Core's attenuator minimum
    /// (0, or -28 on the HL2).
    static constexpr int kMaxAttOnTxDb = 31;

    /// R-R3-49 (parity Task 5): the plain range refusal for a write of one
    /// of the three transmit settings; empty when the value is in range.
    QString transmitSettingRefusal(const QByteArray& property, const QVariant& value) const;
    /// The three transmit settings on this object.
    static bool isTransmitSetting(const QByteArray& property);

    explicit StepAttenuatorFacade(RadioModel* radio, QObject* parent = nullptr);
    ~StepAttenuatorFacade() override;

    /// Follow `controller` (nullptr unbinds; the last values stay).
    void bindController(StepAttenuatorController* controller);
    StepAttenuatorController* controller() const;
    bool isBound() const;

    void setEditGate(EditGate gate) { m_editGate = std::move(gate); }

    /// A remote window: whether its edits can reach the Core now and, when
    /// they cannot, why, in plain words. MainWindow sets it from the session
    /// (R-R3-46, R-R3-21); the RX applet and Setup follow it. Starts
    /// unavailable, with the "connect to the Core" reason.
    void setWindowAvailability(bool available, const QString& reason);
    bool windowAvailable() const { return m_windowAvailable; }
    QString windowUnavailableReason() const { return m_windowReason; }

    /// Why the last edit of `property` settled on another value; empty when
    /// it was taken as asked or was never edited.
    QString settleReason(const QByteArray& property) const;

    /// A remote window: a value the Core reports (minDb, maxDb,
    /// autoAttApplied, overloadAdc0, overloadAdc1, adcLinked,
    /// rx2SliceMask). False for any
    /// other name, and always false while bound.
    bool applyRemoteProperty(const QByteArray& property, const QVariant& value);

    bool enabled() const { return m_values.enabled; }
    int attenuationDb() const { return m_values.attenuationDb; }
    int preampMode() const { return m_values.preampMode; }
    bool rx1Preamp() const { return m_values.rx1Preamp; }
    bool autoAttEnabled() const { return m_values.autoAttEnabled; }
    int autoAttMode() const { return m_values.autoAttMode; }
    bool autoAttUndo() const { return m_values.autoAttUndo; }
    int autoAttUndoDelayMs() const { return m_values.autoAttUndoDelayMs; }
    int autoAttHoldMs() const { return m_values.autoAttHoldMs; }
    bool attOnTxEnabled() const { return m_values.attOnTxEnabled; }
    int attOnTxValue() const { return m_values.attOnTxValue; }
    bool forceAttWhenPsOff() const { return m_values.forceAttWhenPsOff; }
    int rx2AttenuationDb() const { return m_values.rx2AttenuationDb; }
    int rx2SliceMask() const { return m_values.rx2SliceMask; }
    bool rx2StepAttEnabled() const { return m_values.rx2StepAttEnabled; }
    bool rx2AutoAttEnabled() const { return m_values.rx2AutoAttEnabled; }
    bool rx2AutoAttUndo() const { return m_values.rx2AutoAttUndo; }
    int rx2AutoAttUndoDelayMs() const { return m_values.rx2AutoAttUndoDelayMs; }
    int rx2PreampMode() const { return m_values.rx2PreampMode; }
    /// The preamp mode slice `sliceId` hears, and its edit.
    int preampModeForSlice(int sliceId) const
    {
        return sliceUsesRx2(sliceId) ? rx2PreampMode() : preampMode();
    }
    void setPreampModeForSlice(int sliceId, int mode);
    /// Whether slice `sliceId` reads and sets rx2AttenuationDb.
    bool sliceUsesRx2(int sliceId) const
    {
        return sliceId >= 0 && sliceId < 32
            && (static_cast<quint32>(m_values.rx2SliceMask) & (1u << sliceId)) != 0;
    }
    /// The attenuation slice `sliceId` hears, and its edit.
    int attenuationDbForSlice(int sliceId) const
    {
        return sliceUsesRx2(sliceId) ? rx2AttenuationDb() : attenuationDb();
    }
    void setAttenuationDbForSlice(int sliceId, int dB);
    int minDb() const { return m_values.minDb; }
    int maxDb() const { return m_values.maxDb; }
    bool autoAttApplied() const { return m_values.autoAttApplied; }
    int overloadAdc0() const { return m_values.overloadAdc0; }
    int overloadAdc1() const { return m_values.overloadAdc1; }
    bool adcLinked() const { return m_values.adcLinked; }

    void setEnabled(bool on);
    void setAttenuationDb(int dB);
    void setPreampMode(int mode);
    void setRx1Preamp(bool on);
    void setAutoAttEnabled(bool on);
    void setAutoAttMode(int mode);
    void setAutoAttUndo(bool on);
    void setAutoAttUndoDelayMs(int ms);
    void setAutoAttHoldMs(int ms);
    void setAttOnTxEnabled(bool on);
    void setAttOnTxValue(int dB);
    void setForceAttWhenPsOff(bool on);
    void setRx2AttenuationDb(int dB);
    void setRx2StepAttEnabled(bool on);
    void setRx2AutoAttEnabled(bool on);
    void setRx2AutoAttUndo(bool on);
    void setRx2AutoAttUndoDelayMs(int ms);
    void setRx2PreampMode(int mode);

signals:
    void enabledChanged(bool on);
    void attenuationDbChanged(int dB);
    void preampModeChanged(int mode);
    void rx1PreampChanged(bool on);
    void autoAttEnabledChanged(bool on);
    void autoAttModeChanged(int mode);
    void autoAttUndoChanged(bool on);
    void autoAttUndoDelayMsChanged(int ms);
    void autoAttHoldMsChanged(int ms);
    void attOnTxEnabledChanged(bool on);
    void attOnTxValueChanged(int dB);
    void forceAttWhenPsOffChanged(bool on);
    void minDbChanged(int dB);
    void maxDbChanged(int dB);
    void autoAttAppliedChanged(bool applied);
    void overloadAdc0Changed(int level);
    void overloadAdc1Changed(int level);
    void adcLinkedChanged(bool linked);
    void rx2AttenuationDbChanged(int dB);
    void rx2SliceMaskChanged(int mask);
    void rx2StepAttEnabledChanged(bool on);
    void rx2AutoAttEnabledChanged(bool on);
    void rx2AutoAttUndoChanged(bool on);
    void rx2AutoAttUndoDelayMsChanged(int ms);
    void rx2PreampModeChanged(int mode);
    /// An edit the gate refused, with its plain reason.
    void editRejected(const QString& reason);
    /// setWindowAvailability() changed the availability or its reason.
    void windowAvailabilityChanged(bool available);

private:
    struct Values {
        bool enabled{true};
        int attenuationDb{0};
        int preampMode{0};
        bool rx1Preamp{false};
        bool autoAttEnabled{false};
        int autoAttMode{0};
        bool autoAttUndo{false};
        int autoAttUndoDelayMs{5000};
        int autoAttHoldMs{2000};
        // StepAttenuatorController's defaults (m_attOnTxEnabled,
        // m_forceAttWhenPsOff true).
        bool attOnTxEnabled{true};
        int attOnTxValue{0};
        bool forceAttWhenPsOff{true};
        int minDb{0};
        int maxDb{31};
        bool autoAttApplied{false};
        int overloadAdc0{0};
        int overloadAdc1{0};
        bool adcLinked{false};
        int rx2AttenuationDb{0};
        int rx2SliceMask{0};
        bool rx2StepAttEnabled{false};  // Thetis console.cs:11109
        bool rx2AutoAttEnabled{false};
        bool rx2AutoAttUndo{false};
        int rx2AutoAttUndoDelayMs{5000};
        // StepAttenuatorController's m_rx2PreampMode (PreampMode::On).
        int rx2PreampMode{1};
    };

    /// Starts an edit of `property`: clears its settle reason and asks the
    /// gate. False (and editRejected) when the gate refuses.
    bool beginEdit(const char* property);
    void settle(const char* property, const QString& reason);
    /// Re-read the bound controller and emit each property that changed.
    void refresh();
    void publish(const Values& next);

    QPointer<RadioModel> m_radio;
    QPointer<StepAttenuatorController> m_controller;
    QList<QMetaObject::Connection> m_controllerConnections;
    EditGate m_editGate;
    bool m_windowAvailable{false};
    QString m_windowReason;
    QHash<QByteArray, QString> m_settleReasons;
    Values m_values;
};

} // namespace NereusSDR
