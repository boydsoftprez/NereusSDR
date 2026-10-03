#pragma once
// no-port-check: NereusSDR-original. Observational Core/GUI telemetry (R-R3-32).

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <optional>

namespace NereusSDR {

inline constexpr qsizetype kMaxStationAdcOverloads = 3;

// One ADC's observed status on the current radio connection. A missing
// overloaded value is unknown (including a status older than three seconds),
// never an observed clear. The count is transitions into overload, not frames.
struct StationAdcOverloadTelemetry {
    int adc = 0; // 0..2, unique in one sample
    qint64 eventsSinceConnection = 0;
    std::optional<qint64> statusAgeMs;
    std::optional<bool> overloaded;
    std::optional<qint64> lastOverloadAgeMs;
};

// Units and ownership are part of the wire contract. An absent value has not
// been measured; a present zero is an actual measurement. No field grants
// admission, changes radio state or substitutes for heartbeat evidence.
struct StationRadioTelemetry {
    bool connected = false;
    std::optional<double> rxMbps;
    std::optional<double> txMbps;
    std::optional<qint64> rttMs;
    std::optional<qint64> rttAgeMs;
    // R-R3-32 / R-R3-46 (remote-window parity Task 6; session minor 11,
    // stationTelemetryVersion 4). The Core's PA readings as its own window
    // shows them (RadioModel::paReadings()), each absent when the radio has
    // none or has not reported it: the PA drain volts (user ADC0), the
    // supply volts, the PA current, the PA temperature.
    std::optional<double> paVolts;
    std::optional<double> supplyVolts;
    std::optional<double> paCurrentAmps;
    std::optional<double> paTemperatureCelsius;
    // The Core's radio link (RadioConnection::linkStats(), RadioLinkStats):
    // lost / (received + lost) over the last 5 s in percent, RFC 3550
    // interarrival jitter of the lowest receive stream in ms, the longest
    // interval between two datagrams in the last second in ms, the
    // datagrams seen since the radio connected, and the radio's sample
    // rate in Hz.
    std::optional<double> packetLossPercent;
    std::optional<double> jitterMs;
    std::optional<double> packetGapMs;
    std::optional<qint64> sampleRateHz;
    std::optional<qint64> udpPacketsSeen;

    // R-R3-32 (remote-window parity Task 14; session minor 11,
    // stationTelemetryVersion 5). The Core's Hermes Lite 2 link as its own
    // window shows it (RadioModel::bwMonitor(), HermesLiteBandwidthMonitor),
    // each absent unless the Core's radio has the bandwidth monitor (the
    // HL2) and is connected: the bytes per second received from the radio
    // (EP6) and sent to it (EP2), whether the LAN link is throttled, and
    // the EP6 sequence gaps since the radio connected.
    std::optional<double> hl2RxBytesPerSecond;
    std::optional<double> hl2TxBytesPerSecond;
    std::optional<bool> hl2Throttled;
    std::optional<qint64> hl2SequenceGaps;

    // Version 6, minor 11: monotonic age of the model's current connection,
    // the live radio's outbound UDP base/control port, and parsed ADC status.
    // P2 media roles use other ports. An absent ADC boolean is unknown.
    std::optional<qint64> connectionAgeMs;
    std::optional<qint64> radioUdpBasePort;
    std::optional<QVector<StationAdcOverloadTelemetry>> adcOverloads;

    bool hasNoRadioDiagnostics() const
    {
        return !connectionAgeMs && !radioUdpBasePort && !adcOverloads;
    }
    void clearRadioDiagnostics()
    {
        connectionAgeMs.reset(); radioUdpBasePort.reset(); adcOverloads.reset();
    }

    // True when no version 5 field is present.
    bool hasNoHl2Link() const
    {
        return !hl2RxBytesPerSecond && !hl2TxBytesPerSecond && !hl2Throttled
            && !hl2SequenceGaps;
    }
    // Clears every version 5 field (for a peer that did not negotiate it).
    void clearHl2Link()
    {
        hl2RxBytesPerSecond.reset(); hl2TxBytesPerSecond.reset();
        hl2Throttled.reset(); hl2SequenceGaps.reset();
    }

    // True when no version 4 field is present.
    bool hasNoRadioStatus() const
    {
        return !paVolts && !supplyVolts && !paCurrentAmps && !paTemperatureCelsius
            && !packetLossPercent && !jitterMs && !packetGapMs && !sampleRateHz
            && !udpPacketsSeen;
    }
    // Clears every version 4 field (for a peer that did not negotiate it).
    void clearRadioStatus()
    {
        paVolts.reset(); supplyVolts.reset(); paCurrentAmps.reset();
        paTemperatureCelsius.reset(); packetLossPercent.reset(); jitterMs.reset();
        packetGapMs.reset(); sampleRateHz.reset(); udpPacketsSeen.reset();
    }
};

struct StationAudioTelemetry {
    bool active = false;
    quint32 contextGeneration = 0;
    std::optional<double> sourceFramesPerSecond;
    std::optional<double> sourceDropsPerSecond;
    std::optional<double> encodedPacketsPerSecond;
    std::optional<double> encodeFailuresPerSecond;
    // Acceptance by the media transport is NOT evidence of delivery.
    std::optional<double> sendAcceptedPerSecond;
    std::optional<double> sendRejectedPerSecond;
};

// Longest thermal zone name the codec accepts. Linux caps a zone type at
// 20 characters; the sampler truncates anything longer to this.
inline constexpr qsizetype kMaxHostZoneNameLength = 64;

// The Core computer's own load (R-R3-32, R-R3-33). Sent only to a peer that
// negotiated host telemetry (session minor 10, stationTelemetryVersion 2);
// a Core that does not measure a value, including every non-Linux Core,
// leaves it absent. On the wire the section is omitted when nothing in it
// was measured.
//
// Age: the Core samples its host once for every reader (SharedHostSampler,
// shared by telemetry and the display load governor, R-R3-40), so a
// snapshot can carry a reading up to one sampler period old
// (SharedHostSampler::kMinimumIntervalMs, 900 ms), and the CPU percentages
// cover the interval that ended then. Telemetry does not sample afresh:
// that would restart the interval the governor measures.
struct StationHostTelemetry {
    std::optional<double> systemCpuPercent;   // all CPUs, 0-100
    std::optional<double> processCpuPercent;  // nereusd share of all CPUs, 0-100
    std::optional<qint64> memoryAvailableKiB; // MemAvailable
    std::optional<qint64> memoryTotalKiB;     // MemTotal
    std::optional<qint64> processResidentKiB; // VmRSS
    std::optional<double> hottestZoneCelsius;
    QString hottestZoneName;                  // empty when absent

    bool isEmpty() const
    {
        return !systemCpuPercent && !processCpuPercent && !memoryAvailableKiB
            && !memoryTotalKiB && !processResidentKiB && !hottestZoneCelsius
            && hottestZoneName.isEmpty();
    }
};

// Most receivers the codec accepts in one sample. Slice IDs are WDSP channel
// ids, below WdspEngine::kMaxSliceChannels (5); the bound leaves room without
// letting a peer send an unbounded list.
inline constexpr qsizetype kMaxStationReceivers = 16;

// One receiver's processing on the Core over its latest load interval
// (R-R3-40, R-R3-32, R-R3-33). Sent only to a peer that negotiated receiver
// load (session minor 11, stationTelemetryVersion 3).
struct StationReceiverTelemetry {
    int sliceId = 0;                    // 0-65535, unique within a sample
    // Mean processing time per block as a percentage of the block's real
    // time: 100 means the receiver cannot keep up. Absent when the receiver
    // processed nothing in the interval, which is not proof of no load
    // (ReceiverDspLoad::idle).
    std::optional<double> loadPercent;
    qint64 inputDelayMs = 0;            // wait of its latest input batch
    qint64 skippedInputMs = 0;          // input skipped to bound that wait,
                                        // total since the Core started
                                        // processing its receivers
};

struct StationTelemetrySnapshot {
    quint32 sequence = 0;
    // Relative to the producer's session clock. Never subtract from a GUI
    // clock to claim one-way latency; receipt age is measured locally.
    qint64 sampledElapsedMs = 0;
    StationRadioTelemetry radio;
    StationAudioTelemetry audio;
    StationHostTelemetry host;
    // Absent: the Core did not measure its receivers (an older Core, or one
    // with no radio model). Present and empty: it measured, and no receiver
    // had a load reading yet. On the wire an absent list omits "receivers".
    std::optional<QVector<StationReceiverTelemetry>> receivers;
};

namespace StationTelemetryCodec {
// Both functions reject invalid required fields and invalid present optional
// values. Unknown optional JSON fields are ignored. Decode is transactional.
std::optional<QJsonObject> encode(const StationTelemetrySnapshot& snapshot);
bool decode(const QJsonObject& object, StationTelemetrySnapshot* snapshot);
}

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::StationTelemetrySnapshot)
