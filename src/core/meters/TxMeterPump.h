#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/meters/TxMeterPump.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 39 (D14, R-IOS-13): the Core's transmit meters for
// the `txState` object (TransmitState, src/core/session/
// TransmitStateFacade.h), read ten times a second while the radio is on
// the air.
//
// What it reads, and where each reading comes from:
//   forwardPowerWatts, reflectedPowerWatts, swr
//                 RadioStatus, which the radio's power telemetry fills
//                 (RadioModel::handlePaTelemetry): the same values the
//                 desktop's power and SWR meters show.
//   alcDb, micLevelDb
//                 Thetis's ALC and MIC readings (thetisTxReading in
//                 WdspTypes.h, as MeterPoller shows them on the desktop),
//                 worked from TxChannel::txMeter.
//   compressionDb Thetis's COMP reading, worked the same way (parity
//                 Task 33 follow-up, txReadingsVersion 1).
//   eqDb, levelerDb, levelerGainDb, cfcDb, cfcGainDb, alcGainDb,
//   alcGroupDb    Thetis's EQ, LEVELER, LVL_G, CFC_AV, CFC_G, ALC_G and
//                 ALC_GROUP readings, worked the same way: the seven
//                 container meters a remote window shows (A9,
//                 txReadingsVersion 3).
//
// R-R3-39 (the plan's Task 32): TxChannel::txMeter returns the transmit
// lane's last reading and asks the lane for a fresh one, so a poll here
// never makes a WDSP call on the Core's event loop and never waits for the
// lane. With no transmit channel (a Core whose radio has none yet) the ALC
// and MIC readings are kNoReadingDb, the no-reading value the meters show
// as "--".
//
// The poll timer is a child of this object, so a test (and the link's
// conformance runner, whose virtual clock drives the Core's own timers)
// ticks it with the rest of the Core's timers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 39 (D14, R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Remote-window parity Task 33 follow-up (R-R3-49): the COMP
//               reading (compressionDb). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: A9 (iPhone app plan Task 39): the seven stage readings
//               the container meters show (EQ, Leveler, Leveler gain, CFC,
//               CFC gain, ALC gain, ALC group); readFrom. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QMetaType>
#include <QObject>
#include <QPointer>

#include <functional>

class QTimer;

namespace NereusSDR {

class RadioModel;
class RadioStatus;
class TxChannel;
enum class TxMeterType : int;

/// One set of the Core's transmit readings.
struct TxMeterReadings {
    /// No reading (the meters show "--"): the value SliceMeterPump and the
    /// desktop's meters use.
    static constexpr double kNoReadingDb = -400.0;

    double forwardPowerWatts{0.0};
    double reflectedPowerWatts{0.0};
    /// RadioStatus's idle value (RadioStatus::computeSwr: 1.0 with no
    /// forward power).
    double swr{1.0};
    double alcDb{kNoReadingDb};
    double micLevelDb{kNoReadingDb};
    /// Parity Task 33 follow-up: Thetis's COMP reading.
    double compressionDb{kNoReadingDb};
    /// A9 (txReadingsVersion 3): Thetis's EQ, LEVELER, LVL_G, CFC_AV,
    /// CFC_G, ALC_G and ALC_GROUP readings (thetisTxReading), what a local
    /// window's container meters show for TxEq, TxLeveler, TxLevelerGain,
    /// TxCfc, TxCfcGain, TxAlcGain and TxAlcGroup.
    double eqDb{kNoReadingDb};
    double levelerDb{kNoReadingDb};
    double levelerGainDb{kNoReadingDb};
    double cfcDb{kNoReadingDb};
    double cfcGainDb{kNoReadingDb};
    double alcGainDb{kNoReadingDb};
    double alcGroupDb{kNoReadingDb};

    bool operator==(const TxMeterReadings& other) const = default;
};

class TxMeterPump : public QObject {
    Q_OBJECT

public:
    /// Ten readings a second while keyed.
    static constexpr int kIntervalMs = 100;

    using Source = std::function<TxMeterReadings()>;

    /// Reads `model` (not owned; a Core's own model). A null model reads
    /// the idle values.
    explicit TxMeterPump(RadioModel* model, QObject* parent = nullptr);
    ~TxMeterPump() override;

    /// The model a poll reads (not owned).
    void setModel(RadioModel* model);
    /// Test seam: where a poll reads from. Unset, it reads the model.
    void setSource(Source source);

    /// The readings from `status` and `tx` now. `tx` null: no ALC or MIC
    /// reading. Never calls WDSP on the caller's thread while `tx` has a
    /// transmit lane (TxChannel::txMeter).
    static TxMeterReadings read(const RadioStatus& status, const TxChannel* tx);
    /// The same from `readRaw`, one GetTXAMeter reading per WDSP meter
    /// (TxChannel::txMeter); an empty function is no transmit channel.
    static TxMeterReadings readFrom(const RadioStatus& status,
                                    const std::function<double(TxMeterType)>& readRaw);

    /// One reading now, from the source.
    TxMeterReadings readNow() const;

    void start();
    void stop();
    bool isRunning() const;

public slots:
    /// Reads once and emits readingsTaken. The timer's target; public so a
    /// test can tick it.
    void poll();

signals:
    void readingsTaken(const NereusSDR::TxMeterReadings& readings);

private:
    QPointer<RadioModel> m_model;
    QTimer* m_timer{nullptr};
    Source m_source;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::TxMeterReadings)
