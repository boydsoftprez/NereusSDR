#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/TransmitStateFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `txState` object, class TransmitState (iPhone app plan
// Task 39; D14, R-IOS-13, R-IOS-21; spec section 5.5 items 5 and 8): what
// the Core's transmitter is doing, for a remote window and the phone. Every
// property is outbound (station to client). StationServer sends it only at
// agreed minor 11 to a peer whose hello declared remoteTx 1 (txStateVersion
// 1, after remoteTxVersion), so an older peer never receives it.
//
//   keyed                  the radio is on the air (RadioModel
//                          transmitting: MOX, TUNE or two-tone)
//   tuning, twoTone        TUNE / the two-tone test is on
//   txSliceId              the slice transmit is bound to (-1: none)
//   keyedByName, keyedByKind, keyedTrigger
//                          who keyed and how (RadioModel::keyedBy: the
//                          device's name as the Core numbers it, its kind,
//                          its trigger); empty while unkeyed
//   keyedSinceMs           when the key began, on the Core's monotonic
//                          clock in milliseconds; 0 while unkeyed
//   timeOutRemainingSeconds
//                          whole seconds before the transmit time-out stops
//                          the key (Task 38), -1 when none applies
//   forwardPowerWatts, reflectedPowerWatts, swr, alcDb, micLevelDb
//                          the transmit meters (TxMeterPump)
//   txEnding               true only during a RADE end-of-over tail
//                          (RadioModel::endOfOverTailActive)
//   stopReason, stopText   why the Core last stopped a transmission on its
//                          own, and that in the operator's words:
//                          "" (none yet), linkLost, micStarved, timeOut,
//                          takenOver, revoked or station
//   stopSerial             advances by one with each such stop
//   stopEpoch              the keying epoch of the key that stop ended
//                          (the epoch tx.key answered with), so a window
//                          never ends a newer key of its own on an older
//                          stop (fix wave 2, the M7 race); 0 when unknown
//   highSwr, swrWindBackLatched
//                          the Core's high-SWR protection has tripped, and
//                          its drive fold-back has latched: what RadioModel
//                          hands a local window's setHighSwrOverlay
//                          (parity Task 28, txDisplayVersion 1)
//   forwardAdcRaw, reflectedAdcRaw
//                          the radio's raw forward and reflected power
//                          readings (the ADC counts of its last PA sample),
//                          refreshed with the meters, keyed or not; a
//                          older windows scale them as their own PA Values
//                          page does (parity Task 33, txReadingsVersion 1)
//   compressionDb          Thetis's COMP reading (max(-30, TXA_COMP_AV)), with
//                          the meters, as the local Compression meters
//                          show it (Task 33 follow-up, txReadingsVersion 1)
//   forwardRawPowerWatts, forwardAdcVolts, reflectedAdcVolts
//                          Core-scaled PA Values from the current radio's
//                          model and raw ADC sample (txReadingsVersion 2)
//   eqDb, levelerDb, levelerGainDb, cfcDb, cfcGainDb, alcGainDb, alcGroupDb
//                          Thetis's EQ, LEVELER, LVL_G, CFC_AV, CFC_G,
//                          ALC_G and ALC_GROUP readings, with the meters, as
//                          a local window's container meters show them
//                          (A9, txReadingsVersion 3)
//
// Updates: while keyed the meters are read ten times a second (the
// transmit lane's cached readings; never a WDSP call on the event loop)
// and a reading that changed is sent; the time left is read with them.
// While unkeyed only changes are sent (the power meters follow the radio's
// power readings as they change).
//
// Stops: each key (a rising edge of `keyed`) can be stopped once. The
// first reason recorded for it wins and advances stopSerial; a later one
// for the same key changes nothing. The Core's own StopAllTx without a
// reason recorded for it counts as `station`, decided after the reasons
// raised with it (the time-out raises its own just after StopAllTx).
//
// On a remote window the object holds the Core's values as it last heard
// them (applyStationValue); nothing on it is ever written back.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 39 (D14, R-IOS-13,
//               R-IOS-21), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: RADE end-of-over callsigns: txEnding follows
//               RadioModel::endOfOverTailActive; keyedBy* keep the key's
//               holder through an end-of-over tail. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 txState names the holder
//               (holder fields, keyedForSeconds, txStateVersion 2); M3
//               one lost-link sentence. J.J. Boyd (KG4VCF), with AI-
//               assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Parity Task 28 (R-R3-49, A11): highSwr and
//               swrWindBackLatched, the high-SWR state the local window's
//               border shows, appended (txDisplayVersion 1). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: Parity Task 33 (R-R3-49, R-R3-32): forwardAdcRaw and
//               reflectedAdcRaw appended (txReadingsVersion 1), and the
//               txCfcCompression record's bins encoding; then
//               compressionDb, the COMP reading. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: A9 (iPhone app plan Task 39): the seven stage readings
//               (eqDb .. alcGroupDb) appended (txReadingsVersion 3). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-01: Control logging lane: one line per key with the peaks
//               of the leveler, leveler gain, ALC, ALC gain and
//               compression readings the meter pump already took. Logging
//               only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <optional>
#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <functional>

#include "core/meters/TxMeterPump.h"

namespace NereusSDR {

class RadioModel;

class NEREUS_CORE_EXPORT TransmitState final : public QObject {
    Q_OBJECT
    // The wire's order (the plan's Task 39 interface). Do not reorder.
    Q_PROPERTY(bool keyed READ keyed NOTIFY stateChanged)
    Q_PROPERTY(bool tuning READ tuning NOTIFY stateChanged)
    Q_PROPERTY(bool twoTone READ twoTone NOTIFY stateChanged)
    Q_PROPERTY(int txSliceId READ txSliceId NOTIFY stateChanged)
    Q_PROPERTY(QString keyedByName READ keyedByName NOTIFY stateChanged)
    Q_PROPERTY(QString keyedByKind READ keyedByKind NOTIFY stateChanged)
    Q_PROPERTY(QString keyedTrigger READ keyedTrigger NOTIFY stateChanged)
    Q_PROPERTY(qint64 keyedSinceMs READ keyedSinceMs NOTIFY stateChanged)
    Q_PROPERTY(int timeOutRemainingSeconds READ timeOutRemainingSeconds NOTIFY timeOutChanged)
    Q_PROPERTY(double forwardPowerWatts READ forwardPowerWatts NOTIFY metersChanged)
    Q_PROPERTY(double reflectedPowerWatts READ reflectedPowerWatts NOTIFY metersChanged)
    Q_PROPERTY(double swr READ swr NOTIFY metersChanged)
    Q_PROPERTY(double alcDb READ alcDb NOTIFY metersChanged)
    Q_PROPERTY(double micLevelDb READ micLevelDb NOTIFY metersChanged)
    Q_PROPERTY(bool txEnding READ txEnding NOTIFY stateChanged)
    Q_PROPERTY(QString stopReason READ stopReason NOTIFY stopChanged)
    Q_PROPERTY(QString stopText READ stopText NOTIFY stopChanged)
    Q_PROPERTY(quint32 stopSerial READ stopSerial NOTIFY stopChanged)
    // Fix wave I4 (txStateVersion 2; the several-devices design, ruling
    // 8.1): who holds transmit, appended so every earlier ordinal stays.
    Q_PROPERTY(QString holderDeviceId READ holderDeviceId NOTIFY holderChanged)
    Q_PROPERTY(QString holderName READ holderName NOTIFY holderChanged)
    Q_PROPERTY(QString holderShortName READ holderShortName NOTIFY holderChanged)
    Q_PROPERTY(QString holderKind READ holderKind NOTIFY holderChanged)
    Q_PROPERTY(QString holderSource READ holderSource NOTIFY holderChanged)
    Q_PROPERTY(qint64 holderForSeconds READ holderForSeconds NOTIFY holderChanged)
    Q_PROPERTY(qint64 holderEpoch READ holderEpoch NOTIFY holderChanged)
    Q_PROPERTY(bool holderAway READ holderAway NOTIFY holderChanged)
    Q_PROPERTY(bool holderTransferring READ holderTransferring NOTIFY holderChanged)
    // Ruling 10.3: how long the key has been on, in whole seconds, on the
    // Core's clock when it sends it (keyedSinceMs stays for a Core at
    // txStateVersion 1's readers).
    Q_PROPERTY(qint64 keyedForSeconds READ keyedForSeconds NOTIFY stateChanged)
    // Fix wave 2 (the M7 race): the keying epoch of the key the last stop
    // ended, appended so every earlier ordinal stays.
    Q_PROPERTY(qint64 stopEpoch READ stopEpoch NOTIFY stopChanged)
    // Parity Task 28 (txDisplayVersion 1): the high-SWR border's state,
    // appended so every earlier ordinal stays.
    Q_PROPERTY(bool highSwr READ highSwr NOTIFY swrChanged)
    Q_PROPERTY(bool swrWindBackLatched READ swrWindBackLatched NOTIFY swrChanged)
    // Parity Task 33 (txReadingsVersion 1): the radio's raw forward and
    // reflected power readings, appended so every earlier ordinal stays.
    Q_PROPERTY(qint64 forwardAdcRaw READ forwardAdcRaw NOTIFY adcRawChanged)
    Q_PROPERTY(qint64 reflectedAdcRaw READ reflectedAdcRaw NOTIFY adcRawChanged)
    // Task 33 follow-up (txReadingsVersion 1): the COMP reading, appended.
    Q_PROPERTY(double compressionDb READ compressionDb NOTIFY metersChanged)
    // PA Values' existing Core scalers, evaluated beside the raw samples.
    Q_PROPERTY(double forwardRawPowerWatts READ forwardRawPowerWatts NOTIFY adcRawChanged)
    Q_PROPERTY(double forwardAdcVolts READ forwardAdcVolts NOTIFY adcRawChanged)
    Q_PROPERTY(double reflectedAdcVolts READ reflectedAdcVolts NOTIFY adcRawChanged)
    // A9 (txReadingsVersion 3): the container meters' seven stage
    // readings, appended so every earlier ordinal stays.
    Q_PROPERTY(double eqDb READ eqDb NOTIFY metersChanged)
    Q_PROPERTY(double levelerDb READ levelerDb NOTIFY metersChanged)
    Q_PROPERTY(double levelerGainDb READ levelerGainDb NOTIFY metersChanged)
    Q_PROPERTY(double cfcDb READ cfcDb NOTIFY metersChanged)
    Q_PROPERTY(double cfcGainDb READ cfcGainDb NOTIFY metersChanged)
    Q_PROPERTY(double alcGainDb READ alcGainDb NOTIFY metersChanged)
    Q_PROPERTY(double alcGroupDb READ alcGroupDb NOTIFY metersChanged)

public:
    // The link's stopReason values.
    static constexpr const char* kStopLinkLost = "linkLost";
    static constexpr const char* kStopMicStarved = "micStarved";
    static constexpr const char* kStopTimeOut = "timeOut";
    static constexpr const char* kStopTakenOver = "takenOver";
    static constexpr const char* kStopRevoked = "revoked";
    static constexpr const char* kStopStation = "station";

    /// Milliseconds on a monotonic clock.
    using Clock = std::function<qint64()>;

    /// Fix wave I4: the holder of transmit as the Core names it (ruling
    /// 8.1). Empty deviceId (with source empty) while unheld.
    struct Holder {
        /// The device id as connectedDevices sends it, "station" for the
        /// station device.
        QString deviceId;
        QString name;
        QString shortName;
        QString kind;
        /// "device", or "radioPtt" after a take by the radio's own PTT; ""
        /// while unheld.
        QString source;
        /// When it took transmit, on the Core's clock.
        qint64 sinceMs{0};
        quint64 epoch{0};
        bool away{false};
        bool transferring{false};
        bool operator==(const Holder& other) const = default;
    };
    /// The Core's: the holder now (StationServer, from TransmitHolder).
    void setHolder(const Holder& holder);

    /// Unbound: a remote window's copy, or a Core's before bind().
    explicit TransmitState(QObject* parent = nullptr);
    ~TransmitState() override;

    /// The Core's: follows `model` (a model with its own radio) from now
    /// on. Once only.
    void bind(RadioModel* model);
    /// Stops following the model (its owner is going away): no more
    /// signals from it, the pump stopped, the clock dropped.
    void unbind();
    /// The clock keyedSinceMs reads (the Core's device clock). The default
    /// is a monotonic timer started with this object.
    void setClock(Clock clock);
    /// The meters' pump (a child of this object).
    TxMeterPump* meterPump() const { return m_pump; }

    bool keyed() const { return m_keyed; }
    bool tuning() const { return m_tuning; }
    bool twoTone() const { return m_twoTone; }
    int txSliceId() const { return m_txSliceId; }
    QString keyedByName() const { return m_keyedByName; }
    QString keyedByKind() const { return m_keyedByKind; }
    QString keyedTrigger() const { return m_keyedTrigger; }
    qint64 keyedSinceMs() const { return m_keyedSinceMs; }
    int timeOutRemainingSeconds() const { return m_timeOutRemainingSeconds; }
    double forwardPowerWatts() const { return m_meters.forwardPowerWatts; }
    double reflectedPowerWatts() const { return m_meters.reflectedPowerWatts; }
    double swr() const { return m_meters.swr; }
    double alcDb() const { return m_meters.alcDb; }
    double micLevelDb() const { return m_meters.micLevelDb; }
    double compressionDb() const { return m_meters.compressionDb; }
    double eqDb() const { return m_meters.eqDb; }
    double levelerDb() const { return m_meters.levelerDb; }
    double levelerGainDb() const { return m_meters.levelerGainDb; }
    double cfcDb() const { return m_meters.cfcDb; }
    double cfcGainDb() const { return m_meters.cfcGainDb; }
    double alcGainDb() const { return m_meters.alcGainDb; }
    double alcGroupDb() const { return m_meters.alcGroupDb; }
    TxMeterReadings meters() const { return m_meters; }
    bool txEnding() const { return m_txEnding; }
    QString stopReason() const { return m_stopReason; }
    QString stopText() const { return m_stopText; }
    quint32 stopSerial() const { return m_stopSerial; }
    qint64 stopEpoch() const { return m_stopEpoch; }
    bool highSwr() const { return m_highSwr; }
    bool swrWindBackLatched() const { return m_swrWindBackLatched; }
    qint64 forwardAdcRaw() const { return m_forwardAdcRaw; }
    qint64 reflectedAdcRaw() const { return m_reflectedAdcRaw; }
    double forwardRawPowerWatts() const { return m_forwardRawPowerWatts; }
    double forwardAdcVolts() const { return m_forwardAdcVolts; }
    double reflectedAdcVolts() const { return m_reflectedAdcVolts; }

    // ---- Parity Task 33: the txCfcCompression record ----

    /// The record stream's name, its one record's id, and its fields.
    static constexpr const char* kCfcStream = "txCfcCompression";
    static constexpr const char* kCfcRecordId = "0";
    /// The bins as the record carries them: each value rounded to a tenth
    /// of a dB, as a little-endian int16 (tenths), all in base64.
    static QString encodeCfcBins(const double* bins, int count);
    /// Back to dB; empty for text that is not base64 of whole int16s.
    static QList<double> decodeCfcBins(const QString& text);
    /// Why a window has no transmit reading from its Core: a Core below
    /// txReadingsVersion 1 (PA Values, the CFC bar chart).
    static QString txReadingNotSentText();
    QString holderDeviceId() const { return m_holder.deviceId; }
    QString holderName() const { return m_holder.name; }
    QString holderShortName() const { return m_holder.shortName; }
    QString holderKind() const { return m_holder.kind; }
    QString holderSource() const { return m_holder.source; }
    /// Measured now on the Core; the Core's last value in a window.
    qint64 holderForSeconds() const;
    qint64 holderEpoch() const { return static_cast<qint64>(m_holder.epoch); }
    bool holderAway() const { return m_holder.away; }
    bool holderTransferring() const { return m_holder.transferring; }
    /// Measured now on the Core; the Core's last value in a window.
    qint64 keyedForSeconds() const;

    /// The Core stopped the current key (or the one that just ended) on its
    /// own, for `reason` (one of the kStop* values), told as `text`. The
    /// first reason for a key advances stopSerial and returns true; any
    /// later one for the same key, or one before any key, returns false.
    /// Fix wave 2: the stop names the key it ended by its keying epoch,
    /// `epoch` when given, otherwise the Core's keying epoch now.
    bool recordStop(const QByteArray& reason, const QString& text,
                    std::optional<quint32> epoch = std::nullopt);

    /// The name and kind of whoever keyed the current key (or the last
    /// one), kept after the key ends so a stop can name them.
    QString lastKeyedByName() const { return m_lastKeyedByName; }
    QString lastKeyedByKind() const { return m_lastKeyedByKind; }

    // ---- The stop texts (plain words; the phone shows them as sent) ----

    /// "3:00": minutes and seconds.
    static QString durationText(int seconds);
    /// The transmit time-out (Task 38): `which` "mox" or "ping", the limit,
    /// and the kind of device that keyed ("phone" and "tablet" have their
    /// own time-out).
    static QString timeOutText(const QByteArray& which, int limitSeconds,
                               const QString& deviceKind);
    static QString linkLostText(const QString& deviceName);
    static QString micStarvedText(const QString& deviceName);
    static QString revokedText(const QString& deviceName);
    static QString takenOverText(const QString& takerName);
    static QString stationText();

    // ---- A remote window's copy ----

    /// A plain state apply of one of the Core's values. False for a name
    /// this class does not have.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);
    /// Back to the values of a Core with nothing keyed (a window whose Core
    /// is gone, or does not send the object): unkeyed, no time-out, idle
    /// meters. The stop fields keep the last stop heard.
    void clearStationValues();

signals:
    /// keyed, tuning, twoTone, txSliceId, keyedBy*, keyedSinceMs, txEnding.
    void stateChanged();
    void timeOutChanged();
    void metersChanged();
    /// stopReason, stopText, stopSerial, stopEpoch.
    void stopChanged();
    /// Fix wave I4: the holder* properties.
    void holderChanged();
    /// Parity Task 28: highSwr, swrWindBackLatched.
    void swrChanged();
    /// Parity Task 33: forwardAdcRaw, reflectedAdcRaw.
    void adcRawChanged();

private:
    void onTransmittingChanged(bool keyed);
    void onMeterReadings(const TxMeterReadings& readings);
    void onPowerChanged();
    void refreshState();
    void refreshTimeOut();
    void refreshSwr();
    void refreshAdcRaw();
    void setMeters(const TxMeterReadings& readings);
    qint64 now() const;

    QPointer<RadioModel> m_model;
    TxMeterPump* m_pump{nullptr};
    Clock m_clock;
    QElapsedTimer m_monotonic;

    bool m_keyed{false};
    bool m_tuning{false};
    bool m_twoTone{false};
    int m_txSliceId{-1};
    QString m_keyedByName;
    QString m_keyedByKind;
    QString m_keyedTrigger;
    qint64 m_keyedSinceMs{0};
    int m_timeOutRemainingSeconds{-1};
    TxMeterReadings m_meters;
    bool m_txEnding{false};
    QString m_stopReason;
    QString m_stopText;
    quint32 m_stopSerial{0};
    qint64 m_stopEpoch{0};
    bool m_highSwr{false};
    bool m_swrWindBackLatched{false};
    qint64 m_forwardAdcRaw{0};
    qint64 m_reflectedAdcRaw{0};
    double m_forwardRawPowerWatts{0};
    double m_forwardAdcVolts{0};
    double m_reflectedAdcVolts{0};
    // Fix wave I4: the holder; a window's copies of the two durations.
    Holder m_holder;
    qint64 m_stationHolderForSeconds{0};
    qint64 m_stationKeyedForSeconds{0};

    // Control logging lane: this key's stage reading peaks, from the
    // readings the pump already took (kNoReadingDb while none came).
    void logStagePeaks();
    TxMeterReadings m_keyPeaks;
    int m_keyReadings{0};

    // Each rising edge of keyed is a key; the first stop recorded for it
    // wins.
    quint64 m_key{0};
    bool m_keyStopped{false};
    QString m_lastKeyedByName;
    QString m_lastKeyedByKind;
    // The keyedBy this key had, shown through an end-of-over tail after
    // keyedBy clears at the release.
    QString m_heldKeyedByName;
    QString m_heldKeyedByKind;
    QByteArray m_heldKeyedTrigger;
};

} // namespace NereusSDR
