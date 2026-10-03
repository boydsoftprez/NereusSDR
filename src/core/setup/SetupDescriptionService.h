#pragma once
// no-port-check: NereusSDR-original Setup description transport.
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace NereusSDR {

// The string properties are fixed so MirrorSchema can announce the same
// ordinals to every client. Empty strings name categories not published yet.
class SetupDescription final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString general READ general NOTIFY descriptionsChanged)
    Q_PROPERTY(QString hardware READ hardware NOTIFY descriptionsChanged)
    Q_PROPERTY(QString audio READ audio NOTIFY descriptionsChanged)
    // Version 22: DSP also changes alone (the RX buffer sizes' on-the-air
    // lock), so it has its own notify and a key sends DSP and the revision.
    Q_PROPERTY(QString dsp READ dsp NOTIFY dspDescriptionChanged)
    Q_PROPERTY(QString display READ display NOTIFY descriptionsChanged)
    Q_PROPERTY(QString transmit READ transmit NOTIFY descriptionsChanged)
    Q_PROPERTY(QString appearance READ appearance NOTIFY descriptionsChanged)
    Q_PROPERTY(QString catNetwork READ catNetwork NOTIFY descriptionsChanged)
    Q_PROPERTY(QString test READ test NOTIFY descriptionsChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY descriptionsChanged)
    // Version 20: the revision and PA also change alone (the on-the-air
    // lock), so they have their own notify and a key sends PA only.
    Q_PROPERTY(quint32 revision READ revision NOTIFY paDescriptionChanged)
    Q_PROPERTY(QString pa READ pa NOTIFY paDescriptionChanged)
public:
    explicit SetupDescription(QObject* parent = nullptr);

    QJsonObject category(const QString& id) const;
    static bool validateActiveSlicePropertyBinding(const QJsonObject& control);
    static bool validateTransmitPropertyBinding(const QJsonObject& control);
    static bool validateHardwarePropertyBinding(const QJsonObject& control,
                                                HPSDRModel model = HPSDRModel::FIRST);
    static bool validatePaReadoutBinding(const QJsonObject& control);
    static bool validatePaDriveReadoutBinding(const QJsonObject& control);
    static bool validatePaTelemetryReadoutBinding(const QJsonObject& control);
    static bool validatePaBypassBinding(const QJsonObject& control);
    /// Version 13 (R-R3-49): the resource form of a PA or Hardware Config
    /// row, exactly as published; false for any other object.
    static bool validatePaV13Control(const QJsonObject& control);
    /// Version 14: PA Gain's profile rows (paProfileVersion 1).
    static bool validatePaV14Control(const QJsonObject& control);
    static bool validateHardwareV13Control(const QJsonObject& control);
    /// Version 16: HL2 I/O's Hermes Lite Options rows, exactly as published.
    static bool validateHardwareV16Control(const QJsonObject& control);
    /// Version 18: HL2 Options' clock rows, exactly as published.
    static bool validateHardwareV18Control(const QJsonObject& control);
    /// Version 23: Calibration's Rx1 6m LNA row, exactly as published.
    static bool validateHardwareV23Control(const QJsonObject& control);
    /// Version 24: TX Input's Line In Gain in 1.5 dB steps and the Saturn
    /// G2's Mic Tip-Ring row, exactly as published.
    static bool validateAudioV24Control(const QJsonObject& control);
    /// Version 19: DSP > CFC's band editor (`dsp.cfc.bands`), exactly as
    /// published: transmit's cfcProfile, applied with cfc.setProfile.
    static bool validateDspV19Control(const QJsonObject& control);
    /// Version 21: CAT & Network's TCI Forget row greys out while Duplicate
    /// is off. Only that row, with exactly that enabledWhen.
    static bool validateCatNetworkV21EnabledWhen(const QJsonObject& control);
    static bool validateTransmitV13Control(const QJsonObject& control);
    static bool validateTransmitSettingBinding(const QJsonObject& control);
    static bool validateAudioPropertyBinding(const QJsonObject& control);
    static bool validateDspSettingBinding(const QJsonObject& control);
    static bool validateDisplaySettingBinding(const QJsonObject& control);
    static bool validateDisplayPhoneBinding(const QJsonObject& control);
    static bool validateAppearanceColourBinding(const QJsonObject& control);
    static bool validateAppearanceMeterStyleBinding(const QJsonObject& control);
    static bool validateAppearanceResetColours(const QJsonObject& control);
    static bool validateSettingToggleEncoding(const QJsonObject& control);
    static bool validateCommandBinding(const QJsonObject& control, QString* error = nullptr);
    static bool validateTnfTable(const QJsonObject& control, QString* error = nullptr);
    static bool validateAntennaRowsTable(const QJsonObject& control,
                                         HPSDRModel model = HPSDRModel::FIRST);
    static bool validateSettingsHygienePanel(const QJsonObject& control);
    /// Stateless per-session projection of a category string (empty if no ready pages).
    /// Version 20: `holdsTransmit` is whether the peer holds transmit; on
    /// the air it opens PA Gain's transmitting band for that peer alone.
    static QString fitCategoryForVersion(const QString& description, int version,
                                         bool antennaRowsAvailable = true,
                                         bool holdsTransmit = false);
    /// Version 20 (R-R3-49, JJ's ruling: follow Thetis): the Core is on the
    /// air (isCoreOnAir) and the PA band it transmits on
    /// (RadioModel::paOnAirBandIndex, -1 for none). PA Gain's rows then
    /// carry their lock and its reason.
    void setPaOnAirState(bool onAir, int transmittingBand);
    /// The device that holds transmit changed: on the air PA is sent again
    /// so each peer's projection opens or locks the transmitting band.
    void noteTransmitHolderChanged();
    /// Version 22: an on-the-air edge (isCoreOnAir). PA Gain's rows as
    /// setPaOnAirState, and DSP > Options' four RX buffer size rows carry
    /// their lock and its reason, as Thetis greys grpDSPBufferSize while
    /// MOX is on. One revision per edge; PA is sent once.
    void setOnAirState(bool onAir, int transmittingBand);
    void setBoardCapabilities(const BoardCapabilities& caps);
    void setRadioContext(const BoardCapabilities& caps, HPSDRModel model);
    /// Version 13: also the radio Radio Info describes (its name, protocol,
    /// firmware, MAC and address), as the desktop's Radio Info tab shows it.
    void setRadioContext(const BoardCapabilities& caps, HPSDRModel model,
                         const RadioInfo& info);
    quint32 revision() const { return m_revision; }
    QString general() const { return m_general; }
    QString hardware() const { return m_hardware; }
    QString audio() const { return m_audio; }
    QString dsp() const { return m_dsp; }
    QString display() const { return m_display; }
    QString transmit() const { return m_transmit; }
    QString appearance() const { return m_appearance; }
    QString catNetwork() const { return m_catNetwork; }
    QString test() const { return m_test; }
    QString diagnostics() const { return m_diagnostics; }
    QString pa() const { return m_pa; }

signals:
    void descriptionsChanged();
    /// PA and the revision (version 20). Emitted with descriptionsChanged
    /// on a rebuild, and alone for the on-the-air lock.
    void paDescriptionChanged();
    /// DSP (version 22). Emitted with descriptionsChanged on a rebuild, and
    /// with one paDescriptionChanged (the revision) for the on-the-air lock.
    void dspDescriptionChanged();

private:
    void rebuild();
    /// Set the on-air state of one category; true if its text changed.
    /// The caller bumps the revision and emits.
    bool applyDspOnAir(bool onAir);
    bool applyPaOnAir(bool onAir, int transmittingBand);
    BoardCapabilities m_caps{};
    HPSDRModel m_model = HPSDRModel::FIRST;
    RadioInfo m_radioInfo{};
    bool m_paOnAir = false;
    bool m_dspOnAir = false;
    int m_paTransmittingBand = -1;
    quint32 m_revision = 0;
    QString m_general;
    QString m_hardware;
    QString m_audio;
    QString m_dsp;
    QString m_display;
    QString m_transmit;
    QString m_appearance;
    QString m_catNetwork;
    QString m_test;
    QString m_diagnostics;
    QString m_pa;
};

using SetupDescriptionService = SetupDescription;

} // namespace NereusSDR
