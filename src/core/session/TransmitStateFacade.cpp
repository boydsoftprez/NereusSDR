// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/TransmitStateFacade.cpp  (NereusSDR)
// =================================================================
//
// See TransmitStateFacade.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 39 (D14, R-IOS-13,
//               R-IOS-21), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: RADE end-of-over callsigns: txEnding follows
//               RadioModel::endOfOverTailActive. J.J. Boyd (KG4VCF),
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
//   2026-09-26: iPhone app plan Task 77 fix round 2: a key stopped because
//               the Power Genius did not finish switching is a station stop
//               in those words. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 4: a key stopped because
//               the Power Genius went to operate by itself is a station stop
//               in those words. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Parity Task 28 (R-R3-49, A11): highSwr and
//               swrWindBackLatched, the high-SWR state the local window's
//               border shows, appended (txDisplayVersion 1). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: Parity Task 33 (R-R3-49, R-R3-32): forwardAdcRaw and
//               reflectedAdcRaw from RadioModel::paRawAdc (txReadingsVersion
//               1); encodeCfcBins / decodeCfcBins for the txCfcCompression
//               record; compressionDb, the pump's COMP reading. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: A9 (iPhone app plan Task 39): the seven stage readings
//               (txReadingsVersion 3) compared, applied and cleared with
//               the meters. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-01: Control logging lane: one line per key with the peaks
//               of the leveler, leveler gain, ALC, ALC gain and
//               compression readings the meter pump already took. Logging
//               only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include "core/session/TransmitStateFacade.h"

#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioStatus.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/SwrProtectionController.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QMetaObject>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// A device's name for a sentence that names it; "the device" when the
// Core has no name for it.
QString deviceOrDefault(const QString& name)
{
    return name.isEmpty() ? QStringLiteral("the device") : name;
}

// The same at the start of a sentence.
QString leadingDevice(const QString& name)
{
    return name.isEmpty() ? QStringLiteral("The device") : name;
}

bool sameReading(double a, double b)
{
    // A reading that is not a number never equals itself; two of them are
    // the same reading here.
    return a == b || (std::isnan(a) && std::isnan(b));
}

bool sameReadings(const TxMeterReadings& a, const TxMeterReadings& b)
{
    return sameReading(a.forwardPowerWatts, b.forwardPowerWatts)
        && sameReading(a.reflectedPowerWatts, b.reflectedPowerWatts)
        && sameReading(a.swr, b.swr) && sameReading(a.alcDb, b.alcDb)
        && sameReading(a.micLevelDb, b.micLevelDb)
        && sameReading(a.compressionDb, b.compressionDb)
        && sameReading(a.eqDb, b.eqDb) && sameReading(a.levelerDb, b.levelerDb)
        && sameReading(a.levelerGainDb, b.levelerGainDb) && sameReading(a.cfcDb, b.cfcDb)
        && sameReading(a.cfcGainDb, b.cfcGainDb) && sameReading(a.alcGainDb, b.alcGainDb)
        && sameReading(a.alcGroupDb, b.alcGroupDb);
}

} // namespace

TransmitState::TransmitState(QObject* parent)
    : QObject(parent)
    , m_pump(new TxMeterPump(nullptr, this))
{
    m_monotonic.start();
    connect(m_pump, &TxMeterPump::readingsTaken, this, &TransmitState::onMeterReadings);
}

TransmitState::~TransmitState() = default;

void TransmitState::bind(RadioModel* model)
{
    if (model == nullptr || !m_model.isNull()) {
        return;
    }
    m_model = model;
    // The pump reads this model from now on.
    m_pump->setModel(model);

    connect(model, &RadioModel::transmittingChanged, this, &TransmitState::onTransmittingChanged);
    connect(model, &RadioModel::keyedByChanged, this, &TransmitState::refreshState);
    // RADE end-of-over callsigns: txEnding follows the Core's tail.
    connect(model, &RadioModel::endOfOverTailChanged, this, &TransmitState::refreshState);
    connect(&model->transmitModel(), &TransmitModel::tuneChanged, this,
            &TransmitState::refreshState);
    connect(&model->transmitModel(), &TransmitModel::twoToneActiveChanged, this,
            &TransmitState::refreshState);
    if (TxSliceArbiter* arbiter = model->txSliceArbiter()) {
        connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, this,
                &TransmitState::refreshState);
    }
    connect(&model->radioStatus(), &RadioStatus::powerChanged, this,
            &TransmitState::onPowerChanged);
    // Task 38's time-out starts counting on MOX's rising edge, at the end of
    // the key's walk (after `keyed` rose): read the time left then too.
    // Connected after the model's own connection, so the time-out has
    // started when this runs.
    if (MoxController* mox = model->moxController()) {
        connect(mox, &MoxController::moxChanged, this, [this](int, bool, bool) {
            refreshTimeOut();
        });
    }

    // Task 38: the time-out raises its reason just after its StopAllTx.
    connect(model, &RadioModel::transmitStopReasonRaised, this,
            [this](const QByteArray& code, int limitSeconds) {
                if (m_model.isNull()) {
                    return;
                }
                // iPhone app plan Task 77 fix round 2: a key stopped because
                // the Power Genius never reported the state it was sent to
                // (its RF never started): a station stop, in those words.
                if (code == RadioModel::kAmpNotSwitchedStopCode) {
                    recordStop(kStopStation, RadioModel::ampNotSwitchedText());
                    return;
                }
                // Task 77 fix round 4: the amplifier went to operate by
                // itself under the key: a station stop, in those words.
                if (code == RadioModel::kAmpOperatedUnderKeyStopCode) {
                    recordStop(kStopStation, RadioModel::ampOperatedUnderKeyText());
                    return;
                }
                if (code != kStopTimeOut) {
                    return;
                }
                const QByteArray which = m_model->lastTransmitStopReason().which;
                recordStop(code, timeOutText(which, limitSeconds, m_lastKeyedByKind));
            });
    // Task 33: StopAllTx stopped a transmission. A stop with a reason of its
    // own records that reason first (recorded before the stop, or raised
    // right after it, as the time-out's is); this one is decided once those
    // have run, and counts only when none did.
    connect(model, &RadioModel::transmitStopped, this, [this](const QString&) {
        const quint64 key = m_key;
        // Fix wave 2: the key stopped is the one keyed now; a key that
        // starts before the queued call runs has a newer epoch.
        const quint32 epoch = m_model.isNull() ? 0 : m_model->keyingEpoch();
        QMetaObject::invokeMethod(
            this,
            [this, key, epoch]() {
                if (key == m_key) {
                    recordStop(kStopStation, stationText(), epoch);
                }
            },
            Qt::QueuedConnection);
    });

    // Parity Task 28: the high-SWR state RadioModel hands a local window's
    // setHighSwrOverlay (RadioModel.cpp, the SwrProtectionController
    // highSwrChanged and windBackLatchedChanged connects).
    connect(&model->swrProt(), &safety::SwrProtectionController::highSwrChanged, this,
            [this](bool) { refreshSwr(); });
    connect(&model->swrProt(), &safety::SwrProtectionController::windBackLatchedChanged, this,
            [this](bool) { refreshSwr(); });
    refreshSwr();

    // Parity Task 33: the raw PA readings. Unkeyed they follow the radio's
    // samples as they change; keyed the meter pump sets the pace
    // (onMeterReadings).
    connect(model, &RadioModel::paRawAdcChanged, this, [this]() {
        if (!m_keyed) {
            refreshAdcRaw();
        }
    });
    // A new board can use a different curve for the same ADC counts.
    connect(model, &RadioModel::currentRadioChanged, this, [this](const RadioInfo&) {
        refreshAdcRaw();
    });
    refreshAdcRaw();

    // The model as it is now.
    m_meters = m_pump->readNow();
    if (model->isTransmitting()) {
        onTransmittingChanged(true);
    } else {
        refreshState();
        refreshTimeOut();
    }
}

void TransmitState::unbind()
{
    m_pump->stop();
    m_clock = {};
    RadioModel* model = m_model.data();
    if (model != nullptr) {
        disconnect(model, nullptr, this, nullptr);
        disconnect(&model->transmitModel(), nullptr, this, nullptr);
        disconnect(&model->radioStatus(), nullptr, this, nullptr);
        if (TxSliceArbiter* arbiter = model->txSliceArbiter()) {
            disconnect(arbiter, nullptr, this, nullptr);
        }
        if (MoxController* mox = model->moxController()) {
            disconnect(mox, nullptr, this, nullptr);
        }
        disconnect(&model->swrProt(), nullptr, this, nullptr);
    }
    m_pump->setModel(nullptr);
    m_model = nullptr;
    if (m_forwardAdcRaw != 0 || m_reflectedAdcRaw != 0
        || m_forwardRawPowerWatts != 0 || m_forwardAdcVolts != 0
        || m_reflectedAdcVolts != 0) {
        m_forwardAdcRaw = 0;
        m_reflectedAdcRaw = 0;
        m_forwardRawPowerWatts = 0;
        m_forwardAdcVolts = 0;
        m_reflectedAdcVolts = 0;
        emit adcRawChanged();
    }
}

void TransmitState::refreshSwr()
{
    if (m_model.isNull()) {
        return;
    }
    const bool high = m_model->swrProt().highSwr();
    const bool latched = m_model->swrProt().windBackLatched();
    if (high == m_highSwr && latched == m_swrWindBackLatched) {
        return;
    }
    m_highSwr = high;
    m_swrWindBackLatched = latched;
    emit swrChanged();
}

void TransmitState::refreshAdcRaw()
{
    if (m_model.isNull()) {
        return;
    }
    const RadioModel::PaRawAdc raw = m_model->paRawAdc();
    const qint64 forward = raw.forward;
    const qint64 reflected = raw.reflected;
    const HPSDRModel model = m_model->hardwareProfile().model;
    const double rawPower = scaleFwdPowerWatts(model, raw.forward);
    const double forwardVolts = scaleFwdRevVoltage(model, raw.forward);
    const double reflectedVolts = scaleFwdRevVoltage(model, raw.reflected);
    if (forward == m_forwardAdcRaw && reflected == m_reflectedAdcRaw
        && rawPower == m_forwardRawPowerWatts && forwardVolts == m_forwardAdcVolts
        && reflectedVolts == m_reflectedAdcVolts) {
        return;
    }
    m_forwardAdcRaw = forward;
    m_reflectedAdcRaw = reflected;
    m_forwardRawPowerWatts = rawPower;
    m_forwardAdcVolts = forwardVolts;
    m_reflectedAdcVolts = reflectedVolts;
    emit adcRawChanged();
}

QString TransmitState::encodeCfcBins(const double* bins, int count)
{
    QByteArray bytes;
    bytes.reserve(count * 2);
    for (int i = 0; i < count; ++i) {
        const double v = bins[i];
        const double tenths = std::isfinite(v) ? std::round(v * 10.0) : 0.0;
        const auto clamped = static_cast<qint16>(std::clamp(tenths, -32768.0, 32767.0));
        const auto u = static_cast<quint16>(clamped);
        bytes.append(static_cast<char>(u & 0xFF));
        bytes.append(static_cast<char>((u >> 8) & 0xFF));
    }
    return QString::fromLatin1(bytes.toBase64());
}

QList<double> TransmitState::decodeCfcBins(const QString& text)
{
    const QByteArray::FromBase64Result decoded =
        QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() % 2 != 0) {
        return {};
    }
    const QByteArray& bytes = decoded.decoded;
    QList<double> bins;
    bins.reserve(bytes.size() / 2);
    for (qsizetype i = 0; i + 1 < bytes.size(); i += 2) {
        const auto u = static_cast<quint16>(static_cast<quint8>(bytes.at(i))
                                            | (static_cast<quint8>(bytes.at(i + 1)) << 8));
        bins.append(static_cast<qint16>(u) / 10.0);
    }
    return bins;
}

void TransmitState::setClock(Clock clock)
{
    m_clock = std::move(clock);
}

qint64 TransmitState::now() const
{
    return m_clock ? m_clock() : m_monotonic.elapsed();
}

void TransmitState::onTransmittingChanged(bool keyed)
{
    if (m_model.isNull()) {
        return;
    }
    if (keyed) {
        // A new key: it can be stopped once.
        ++m_key;
        m_keyStopped = false;
        m_keyedSinceMs = now();
        m_keyPeaks = {};
        m_keyReadings = 0;
        refreshState();
        refreshTimeOut();
        m_pump->poll();
        m_pump->start();
        return;
    }
    m_pump->stop();
    logStagePeaks();
    m_keyedSinceMs = 0;
    refreshState();
    refreshTimeOut();
    // The power meters as the radio reads them now; the ALC and MIC
    // readings keep their last values.
    onPowerChanged();
}

void TransmitState::refreshState()
{
    if (m_model.isNull()) {
        return;
    }
    const bool keyed = m_model->isTransmitting();
    const bool tuning = m_model->transmitModel().isTune();
    const bool twoTone = m_model->transmitModel().isTwoToneActive();
    const TxSliceArbiter* arbiter = m_model->txSliceArbiter();
    const int txSliceId = arbiter != nullptr ? arbiter->txBoundSliceId() : -1;
    const RadioModel::KeyedBy keyedBy = m_model->keyedBy();
    // RADE end-of-over callsigns (review Minor 3): keyedBy clears at the
    // release, but the radio stays on the air through an end-of-over tail.
    // During the tail who keyed stays the one this key had. (Only then: the
    // ordinary TX to RX handover keeps its recorded wire behaviour.)
    if (!keyed) {
        m_heldKeyedByName.clear();
        m_heldKeyedByKind.clear();
        m_heldKeyedTrigger.clear();
    } else if (!keyedBy.isEmpty()) {
        m_heldKeyedByName = keyedBy.deviceName;
        m_heldKeyedByKind = keyedBy.deviceKind;
        m_heldKeyedTrigger = keyedBy.trigger;
    }
    const bool ending = m_model->endOfOverTailActive();
    const bool held = keyedBy.isEmpty() && ending;
    // Who keyed, only while keyed (keyedBy is empty while unkeyed).
    const QString name = !keyed ? QString() : held ? m_heldKeyedByName : keyedBy.deviceName;
    const QString kind = !keyed ? QString() : held ? m_heldKeyedByKind : keyedBy.deviceKind;
    const QString trigger = !keyed ? QString()
        : QString::fromLatin1(held ? m_heldKeyedTrigger : keyedBy.trigger);
    const qint64 since = keyed ? m_keyedSinceMs : 0;
    if (keyed && !keyedBy.isEmpty()) {
        m_lastKeyedByName = keyedBy.deviceName;
        m_lastKeyedByKind = keyedBy.deviceKind;
    }
    if (keyed == m_keyed && tuning == m_tuning && twoTone == m_twoTone
        && txSliceId == m_txSliceId && name == m_keyedByName && kind == m_keyedByKind
        && trigger == m_keyedTrigger && since == m_keyedSinceMs && ending == m_txEnding) {
        return;
    }
    m_keyed = keyed;
    m_tuning = tuning;
    m_twoTone = twoTone;
    m_txSliceId = txSliceId;
    m_keyedByName = name;
    m_keyedByKind = kind;
    m_keyedTrigger = trigger;
    m_keyedSinceMs = since;
    m_txEnding = ending;
    emit stateChanged();
}

void TransmitState::refreshTimeOut()
{
    if (m_model.isNull()) {
        return;
    }
    const int remaining = m_keyed ? m_model->timeOutRemainingSeconds() : -1;
    if (remaining == m_timeOutRemainingSeconds) {
        return;
    }
    m_timeOutRemainingSeconds = remaining;
    emit timeOutChanged();
}

void TransmitState::onMeterReadings(const TxMeterReadings& readings)
{
    // Control logging lane: the key's peaks, from this reading.
    ++m_keyReadings;
    m_keyPeaks.levelerDb = std::max(m_keyPeaks.levelerDb, readings.levelerDb);
    m_keyPeaks.levelerGainDb = std::max(m_keyPeaks.levelerGainDb, readings.levelerGainDb);
    m_keyPeaks.alcDb = std::max(m_keyPeaks.alcDb, readings.alcDb);
    m_keyPeaks.alcGainDb = std::max(m_keyPeaks.alcGainDb, readings.alcGainDb);
    m_keyPeaks.compressionDb = std::max(m_keyPeaks.compressionDb, readings.compressionDb);
    setMeters(readings);
    // Parity Task 33: the raw PA readings at the meters' pace while keyed.
    refreshAdcRaw();
    refreshTimeOut();
}

void TransmitState::logStagePeaks()
{
    if (m_keyReadings == 0) {
        return;
    }
    const auto peak = [](double db) {
        return db <= TxMeterReadings::kNoReadingDb ? QStringLiteral("none")
                                                   : QStringLiteral("%1 dB").arg(db, 0, 'f', 1);
    };
    qCInfo(lcDsp).noquote()
        << QStringLiteral("TX stage peaks for key %1 (%2 readings): leveler %3, leveler gain %4, "
                          "ALC %5, ALC gain %6, compression %7")
               .arg(m_key)
               .arg(m_keyReadings)
               .arg(peak(m_keyPeaks.levelerDb), peak(m_keyPeaks.levelerGainDb),
                    peak(m_keyPeaks.alcDb), peak(m_keyPeaks.alcGainDb),
                    peak(m_keyPeaks.compressionDb));
    m_keyReadings = 0;
}

void TransmitState::onPowerChanged()
{
    // While keyed the pump sets the pace; unkeyed, the power meters follow
    // the radio's readings as they change.
    if (m_model.isNull() || m_keyed) {
        return;
    }
    const RadioStatus& status = m_model->radioStatus();
    TxMeterReadings readings = m_meters;
    readings.forwardPowerWatts = status.forwardPowerWatts();
    readings.reflectedPowerWatts = status.reflectedPowerWatts();
    readings.swr = status.swrRatio();
    setMeters(readings);
}

void TransmitState::setMeters(const TxMeterReadings& readings)
{
    if (sameReadings(readings, m_meters)) {
        return;
    }
    m_meters = readings;
    emit metersChanged();
}

void TransmitState::setHolder(const Holder& holder)
{
    if (holder == m_holder) {
        return;
    }
    m_holder = holder;
    emit holderChanged();
}

qint64 TransmitState::holderForSeconds() const
{
    if (m_model.isNull()) {
        return m_stationHolderForSeconds;
    }
    // Ruling 10.3: whole seconds on the Core's clock, measured as it sends.
    return m_holder.deviceId.isEmpty() ? 0 : std::max<qint64>(0, (now() - m_holder.sinceMs) / 1000);
}

qint64 TransmitState::keyedForSeconds() const
{
    if (m_model.isNull()) {
        return m_stationKeyedForSeconds;
    }
    return m_keyed ? std::max<qint64>(0, (now() - m_keyedSinceMs) / 1000) : 0;
}

bool TransmitState::recordStop(const QByteArray& reason, const QString& text,
                               std::optional<quint32> epoch)
{
    if (m_key == 0 || m_keyStopped) {
        return false;
    }
    m_keyStopped = true;
    m_stopReason = QString::fromLatin1(reason);
    m_stopText = text;
    m_stopEpoch = epoch.has_value() ? static_cast<qint64>(*epoch)
                  : m_model.isNull() ? 0
                                     : static_cast<qint64>(m_model->keyingEpoch());
    ++m_stopSerial;
    emit stopChanged();
    return true;
}

// ---- The stop texts ----

QString TransmitState::durationText(int seconds)
{
    const int whole = std::max(0, seconds);
    return QStringLiteral("%1:%2").arg(whole / 60).arg(whole % 60, 2, 10, QLatin1Char('0'));
}

QString TransmitState::timeOutText(const QByteArray& which, int limitSeconds,
                                   const QString& deviceKind)
{
    const QString after = durationText(limitSeconds);
    if (which == QByteArrayLiteral("ping")) {
        return QStringLiteral("Transmit stopped: the Core's network check went unanswered "
                              "for %1.")
            .arg(after);
    }
    if (deviceKind == QLatin1String("phone") || deviceKind == QLatin1String("tablet")) {
        return QStringLiteral("Transmit stopped after %1, the Core's time-out for phones and "
                              "tablets.")
            .arg(after);
    }
    return QStringLiteral("Transmit stopped after %1, the Core's transmit time-out.").arg(after);
}

QString TransmitState::linkLostText(const QString& deviceName)
{
    // Fix wave M3: one sentence for a lost link, the watchdog's, in the
    // log, the toast and here alike.
    return RemoteTxWatchdog::stopMessage(deviceOrDefault(deviceName));
}

QString TransmitState::micStarvedText(const QString& deviceName)
{
    return QStringLiteral("No microphone audio arrived from %1, so the Core stopped "
                          "transmitting.")
        .arg(deviceOrDefault(deviceName));
}

QString TransmitState::revokedText(const QString& deviceName)
{
    return QStringLiteral("%1 was removed from the Core, so the Core stopped transmitting.")
        .arg(leadingDevice(deviceName));
}

QString TransmitState::takenOverText(const QString& takerName)
{
    return QStringLiteral("%1 took transmit, so the Core stopped transmitting.")
        .arg(leadingDevice(takerName));
}

QString TransmitState::txReadingNotSentText()
{
    return QStringLiteral("This Core does not send this reading. Updating the Core may help.");
}

QString TransmitState::stationText()
{
    return QStringLiteral("The Core stopped transmitting.");
}

// ---- A remote window's copy ----

bool TransmitState::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    bool state = false;
    bool timeOut = false;
    bool meters = false;
    bool stop = false;
    TxMeterReadings readings = m_meters;
    if (propertyName == "keyed") {
        state = value.toBool() != m_keyed;
        m_keyed = value.toBool();
    } else if (propertyName == "tuning") {
        state = value.toBool() != m_tuning;
        m_tuning = value.toBool();
    } else if (propertyName == "twoTone") {
        state = value.toBool() != m_twoTone;
        m_twoTone = value.toBool();
    } else if (propertyName == "txSliceId") {
        state = value.toInt() != m_txSliceId;
        m_txSliceId = value.toInt();
    } else if (propertyName == "keyedByName") {
        state = value.toString() != m_keyedByName;
        m_keyedByName = value.toString();
    } else if (propertyName == "keyedByKind") {
        state = value.toString() != m_keyedByKind;
        m_keyedByKind = value.toString();
    } else if (propertyName == "keyedTrigger") {
        state = value.toString() != m_keyedTrigger;
        m_keyedTrigger = value.toString();
    } else if (propertyName == "keyedSinceMs") {
        state = value.toLongLong() != m_keyedSinceMs;
        m_keyedSinceMs = value.toLongLong();
    } else if (propertyName == "txEnding") {
        state = value.toBool() != m_txEnding;
        m_txEnding = value.toBool();
    } else if (propertyName == "timeOutRemainingSeconds") {
        timeOut = value.toInt() != m_timeOutRemainingSeconds;
        m_timeOutRemainingSeconds = value.toInt();
    } else if (propertyName == "forwardPowerWatts") {
        readings.forwardPowerWatts = value.toDouble();
        meters = true;
    } else if (propertyName == "reflectedPowerWatts") {
        readings.reflectedPowerWatts = value.toDouble();
        meters = true;
    } else if (propertyName == "swr") {
        readings.swr = value.toDouble();
        meters = true;
    } else if (propertyName == "alcDb") {
        readings.alcDb = value.toDouble();
        meters = true;
    } else if (propertyName == "micLevelDb") {
        readings.micLevelDb = value.toDouble();
        meters = true;
    } else if (propertyName == "compressionDb") {
        // Parity Task 33 follow-up: the Core's COMP reading.
        readings.compressionDb = value.toDouble();
        meters = true;
    } else if (double* stage = propertyName == "eqDb"        ? &readings.eqDb
                             : propertyName == "levelerDb"     ? &readings.levelerDb
                             : propertyName == "levelerGainDb" ? &readings.levelerGainDb
                             : propertyName == "cfcDb"         ? &readings.cfcDb
                             : propertyName == "cfcGainDb"     ? &readings.cfcGainDb
                             : propertyName == "alcGainDb"     ? &readings.alcGainDb
                             : propertyName == "alcGroupDb"    ? &readings.alcGroupDb
                                                               : nullptr) {
        // A9 (txReadingsVersion 3): the Core's seven stage readings.
        *stage = value.toDouble();
        meters = true;
    } else if (propertyName == "stopReason") {
        stop = value.toString() != m_stopReason;
        m_stopReason = value.toString();
    } else if (propertyName == "stopText") {
        stop = value.toString() != m_stopText;
        m_stopText = value.toString();
    } else if (propertyName == "stopSerial") {
        const quint32 serial = static_cast<quint32>(value.toLongLong());
        stop = serial != m_stopSerial;
        m_stopSerial = serial;
    } else if (propertyName == "stopEpoch") {
        stop = value.toLongLong() != m_stopEpoch;
        m_stopEpoch = value.toLongLong();
    } else if (propertyName == "highSwr" || propertyName == "swrWindBackLatched") {
        // Parity Task 28: the Core's high-SWR state.
        bool& field = propertyName == "highSwr" ? m_highSwr : m_swrWindBackLatched;
        if (value.toBool() != field) {
            field = value.toBool();
            emit swrChanged();
        }
        return true;
    } else if (propertyName == "forwardAdcRaw" || propertyName == "reflectedAdcRaw") {
        // Parity Task 33: the radio's raw PA readings, as the Core sends them.
        qint64& field = propertyName == "forwardAdcRaw" ? m_forwardAdcRaw : m_reflectedAdcRaw;
        if (value.toLongLong() != field) {
            field = value.toLongLong();
            emit adcRawChanged();
        }
        return true;
    } else if (propertyName == "forwardRawPowerWatts" || propertyName == "forwardAdcVolts"
               || propertyName == "reflectedAdcVolts") {
        double& field = propertyName == "forwardRawPowerWatts" ? m_forwardRawPowerWatts
            : propertyName == "forwardAdcVolts" ? m_forwardAdcVolts : m_reflectedAdcVolts;
        if (value.toDouble() != field) {
            field = value.toDouble();
            emit adcRawChanged();
        }
        return true;
    } else if (propertyName == "keyedForSeconds") {
        state = value.toLongLong() != m_stationKeyedForSeconds;
        m_stationKeyedForSeconds = value.toLongLong();
    } else if (propertyName.startsWith("holder")) {
        // Fix wave I4: the holder, as the Core sends it.
        Holder next = m_holder;
        qint64 forSeconds = m_stationHolderForSeconds;
        if (propertyName == "holderDeviceId") {
            next.deviceId = value.toString();
        } else if (propertyName == "holderName") {
            next.name = value.toString();
        } else if (propertyName == "holderShortName") {
            next.shortName = value.toString();
        } else if (propertyName == "holderKind") {
            next.kind = value.toString();
        } else if (propertyName == "holderSource") {
            next.source = value.toString();
        } else if (propertyName == "holderForSeconds") {
            forSeconds = value.toLongLong();
        } else if (propertyName == "holderEpoch") {
            next.epoch = static_cast<quint64>(value.toLongLong());
        } else if (propertyName == "holderAway") {
            next.away = value.toBool();
        } else if (propertyName == "holderTransferring") {
            next.transferring = value.toBool();
        } else {
            return false;
        }
        if (!(next == m_holder) || forSeconds != m_stationHolderForSeconds) {
            m_holder = next;
            m_stationHolderForSeconds = forSeconds;
            emit holderChanged();
        }
        return true;
    } else {
        return false;
    }
    if (state) {
        emit stateChanged();
    }
    if (timeOut) {
        emit timeOutChanged();
    }
    if (meters) {
        setMeters(readings);
    }
    if (stop) {
        emit stopChanged();
    }
    return true;
}

void TransmitState::clearStationValues()
{
    const bool state = m_keyed || m_tuning || m_twoTone || m_txSliceId != -1
        || !m_keyedByName.isEmpty() || !m_keyedByKind.isEmpty() || !m_keyedTrigger.isEmpty()
        || m_keyedSinceMs != 0 || m_txEnding;
    m_keyed = false;
    m_tuning = false;
    m_twoTone = false;
    m_txSliceId = -1;
    m_keyedByName.clear();
    m_keyedByKind.clear();
    m_keyedTrigger.clear();
    m_keyedSinceMs = 0;
    m_stationKeyedForSeconds = 0;
    m_txEnding = false;
    if (state) {
        emit stateChanged();
    }
    // Fix wave I4: nobody holds transmit on a Core that is gone.
    if (!(m_holder == Holder{}) || m_stationHolderForSeconds != 0) {
        m_holder = Holder{};
        m_stationHolderForSeconds = 0;
        emit holderChanged();
    }
    if (m_timeOutRemainingSeconds != -1) {
        m_timeOutRemainingSeconds = -1;
        emit timeOutChanged();
    }
    setMeters(TxMeterReadings{});
    if (m_forwardAdcRaw != 0 || m_reflectedAdcRaw != 0
        || m_forwardRawPowerWatts != 0 || m_forwardAdcVolts != 0
        || m_reflectedAdcVolts != 0) {
        m_forwardAdcRaw = 0;
        m_reflectedAdcRaw = 0;
        m_forwardRawPowerWatts = 0;
        m_forwardAdcVolts = 0;
        m_reflectedAdcVolts = 0;
        emit adcRawChanged();
    }
    if (m_highSwr || m_swrWindBackLatched) {
        m_highSwr = false;
        m_swrWindBackLatched = false;
        emit swrChanged();
    }
    // The stop fields stay: they say what happened, and the next Core's
    // values replace them.
}

} // namespace NereusSDR
