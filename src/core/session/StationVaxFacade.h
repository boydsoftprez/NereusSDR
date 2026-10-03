#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationVaxFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `vax` object, class StationVax (iPhone app plan Task 25,
// R-IOS-18): the station computer's VAX channels as its own VAX applet
// shows them (src/gui/applets/VaxApplet.cpp), for the phone and any other
// device that asks. It exists on a Core whose audio engine publishes VAX
// devices (a Core the desktop hosts; nereusd publishes none, R-R3-44), and
// StationServer sends it only to a peer whose hello declared `vax` 1
// (vaxVersion 1), so an older peer never receives it.
//
// Properties, in wire order (appended only):
//   ch1Slices .. ch4Slices   the letters of the slices feeding each channel,
//                            in slice order ("AB"), "" when none (outbound)
//   ch1RxGain .. ch4RxGain   each channel's receive level, 0 to 1
//                            (bidirectional; AudioEngine::setVaxRxGain)
//   ch1Muted .. ch4Muted     each channel's mute (bidirectional;
//                            AudioEngine::setVaxMuted)
//   ch1Device .. ch4Device   the channel's device name as the applet shows
//                            it (outbound)
//   txSlice                  the transmit slice's letter, "" when none
//                            (outbound)
//   txGain                   the level of VAX used as the microphone, 0 to
//                            1 (bidirectional; AudioEngine::setVaxTxGain).
//                            StationServer takes a write only from a device
//                            that may transmit.
//
// A write from a device applies to the Core's audio engine at once, as the
// applet's controls do, and is saved under the applet's own keys, so the
// local applet shows it and it outlives a restart. A change made on the
// station computer (its applet) reaches every device through the engine's
// signals. The meters are not properties: they travel as the `vaxLevels`
// record stream, 5 times a second while a device subscribes.
//
// In a remote window (StationClient) the same class, unbound, is the
// window's copy of the Core's: the station's values land through the
// writable properties' setters and applyStationValue(), and a window's own
// change through a setter is sent to the Core as a property write. The
// window's VAX applet shows it in its "Station computer" section.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 25 (R-IOS-18), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: the unbound copy a remote window holds (stored values,
//               applyStationValue, clearStationValues, levels). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QObject>
#include <QPointer>
#include <QString>

namespace NereusSDR {

class AppSettings;
class AudioEngine;
class RadioModel;

class StationVax final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString ch1Slices READ ch1Slices NOTIFY slicesChanged)
    Q_PROPERTY(QString ch2Slices READ ch2Slices NOTIFY slicesChanged)
    Q_PROPERTY(QString ch3Slices READ ch3Slices NOTIFY slicesChanged)
    Q_PROPERTY(QString ch4Slices READ ch4Slices NOTIFY slicesChanged)
    Q_PROPERTY(double ch1RxGain READ ch1RxGain WRITE setCh1RxGain NOTIFY gainsChanged)
    Q_PROPERTY(double ch2RxGain READ ch2RxGain WRITE setCh2RxGain NOTIFY gainsChanged)
    Q_PROPERTY(double ch3RxGain READ ch3RxGain WRITE setCh3RxGain NOTIFY gainsChanged)
    Q_PROPERTY(double ch4RxGain READ ch4RxGain WRITE setCh4RxGain NOTIFY gainsChanged)
    Q_PROPERTY(bool ch1Muted READ ch1Muted WRITE setCh1Muted NOTIFY mutesChanged)
    Q_PROPERTY(bool ch2Muted READ ch2Muted WRITE setCh2Muted NOTIFY mutesChanged)
    Q_PROPERTY(bool ch3Muted READ ch3Muted WRITE setCh3Muted NOTIFY mutesChanged)
    Q_PROPERTY(bool ch4Muted READ ch4Muted WRITE setCh4Muted NOTIFY mutesChanged)
    Q_PROPERTY(QString ch1Device READ ch1Device NOTIFY devicesChanged)
    Q_PROPERTY(QString ch2Device READ ch2Device NOTIFY devicesChanged)
    Q_PROPERTY(QString ch3Device READ ch3Device NOTIFY devicesChanged)
    Q_PROPERTY(QString ch4Device READ ch4Device NOTIFY devicesChanged)
    Q_PROPERTY(QString txSlice READ txSlice NOTIFY slicesChanged)
    Q_PROPERTY(double txGain READ txGain WRITE setTxGain NOTIFY gainsChanged)

public:
    static constexpr int kChannels = 4;
    /// The record stream carrying the meters, one record, id "0".
    static constexpr const char* kLevelsStream = "vaxLevels";
    static constexpr const char* kLevelsRecordId = "0";
    /// The meters' pace while a device subscribes: 5 times a second.
    static constexpr int kLevelsIntervalMs = 200;

    /// The applet's saved keys (VaxApplet.cpp), which this object writes too.
    static QString rxGainKey(int channel);
    static QString mutedKey(int channel);
    static QString txGainKey();
    /// The device name the applet shows for `channel` (1..4).
    static QString deviceName(int channel);
    /// A property this object takes from a device only from one that may
    /// transmit.
    static bool isTransmitProperty(const QByteArray& name);
    /// Why a level write is refused: empty for a finite value from 0 to 1.
    static QString levelRefusal(const QByteArray& name, const QVariant& value);

    explicit StationVax(QObject* parent = nullptr);
    ~StationVax() override;

    /// Follow `model`'s slices and `audio`'s VAX state, saving a device's
    /// writes into `settings`. None owned; any may be null.
    void bind(RadioModel* model, AudioEngine* audio, AppSettings* settings);

    QString ch1Slices() const { return m_slices[0]; }
    QString ch2Slices() const { return m_slices[1]; }
    QString ch3Slices() const { return m_slices[2]; }
    QString ch4Slices() const { return m_slices[3]; }
    double ch1RxGain() const { return rxGain(1); }
    double ch2RxGain() const { return rxGain(2); }
    double ch3RxGain() const { return rxGain(3); }
    double ch4RxGain() const { return rxGain(4); }
    bool ch1Muted() const { return muted(1); }
    bool ch2Muted() const { return muted(2); }
    bool ch3Muted() const { return muted(3); }
    bool ch4Muted() const { return muted(4); }
    QString ch1Device() const { return device(1); }
    QString ch2Device() const { return device(2); }
    QString ch3Device() const { return device(3); }
    QString ch4Device() const { return device(4); }
    /// Channel 1..4's device name: this computer's on the Core, the Core's
    /// in a window's copy.
    QString device(int channel) const;
    QString txSlice() const { return m_txSlice; }
    double txGain() const;

    void setCh1RxGain(double gain) { setRxGain(1, gain); }
    void setCh2RxGain(double gain) { setRxGain(2, gain); }
    void setCh3RxGain(double gain) { setRxGain(3, gain); }
    void setCh4RxGain(double gain) { setRxGain(4, gain); }
    void setCh1Muted(bool on) { setMuted(1, on); }
    void setCh2Muted(bool on) { setMuted(2, on); }
    void setCh3Muted(bool on) { setMuted(3, on); }
    void setCh4Muted(bool on) { setMuted(4, on); }
    void setTxGain(double gain);

    double rxGain(int channel) const;
    bool muted(int channel) const;
    void setRxGain(int channel, double gain);
    void setMuted(int channel, bool on);

    // ---- A remote window's copy (unbound) ----

    /// The Core's value for an outbound property (the slices, the device
    /// names, the transmit slice). False for a name it does not hold.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);
    /// Back to the values of a Core that sent nothing.
    void clearStationValues();
    /// The levels of channels 1..4 and the transmit level as the Core's
    /// vaxLevels stream last said (a window's copy only).
    void setStationLevels(const double* rx, double tx);
    double stationLevel(int channel) const;
    double stationTxLevel() const { return m_txLevel; }

signals:
    void slicesChanged();
    void gainsChanged();
    void mutesChanged();
    void devicesChanged();
    /// A window's copy: new levels from the Core's vaxLevels stream.
    void levelsChanged();

private:
    void watchSlice(QObject* slice);
    void refreshSlices();
    void save(const QString& key, const QString& value);

    QPointer<RadioModel> m_model;
    QPointer<AudioEngine> m_audio;
    AppSettings* m_settings{nullptr};
    QString m_slices[kChannels];
    QString m_txSlice;
    // The window's copy: the values while unbound.
    double m_rxGain[kChannels]{1.0, 1.0, 1.0, 1.0};
    bool m_muted[kChannels]{};
    double m_txGainCopy{1.0};
    QString m_devices[kChannels];
    double m_levels[kChannels]{};
    double m_txLevel{0.0};
};

} // namespace NereusSDR
