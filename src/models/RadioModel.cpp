#include "RadioModel.h"
#include "RxDspWorker.h"
#include "core/RadioConnection.h"
#include "core/RadioConnectionTeardown.h"
#include "core/RadioDiscovery.h"
#include "core/BoardCapabilities.h"
#include "core/ReceiverManager.h"
#include "core/AudioEngine.h"
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/SpectrumWidget.h"

#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

RadioModel::RadioModel(QObject* parent)
    : QObject(parent)
    , m_discovery(new RadioDiscovery(this))
    , m_receiverManager(new ReceiverManager(this))
    , m_audioEngine(new AudioEngine(this))
    , m_wdspEngine(new WdspEngine(this))
{
    // Connection starts null — created by connectToRadio() via factory.
    //
    // Phase 3G-9b: the smooth-defaults profile is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage per user
    // decision 2026-04-15 (Default should stay the out-of-box default).
    // No first-launch auto-apply here. The `DisplayProfileApplied`
    // AppSettings key is reserved for PR3 (Clarity) to repurpose.
}

RadioModel::~RadioModel()
{
    teardownConnection();
    qDeleteAll(m_slices);
    qDeleteAll(m_panadapters);
}

bool RadioModel::isConnected() const
{
    return m_connection && m_connection->isConnected();
}

// --- Slice Management ---

SliceModel* RadioModel::sliceAt(int index) const
{
    if (index >= 0 && index < m_slices.size()) {
        return m_slices.at(index);
    }
    return nullptr;
}

int RadioModel::addSlice()
{
    auto* slice = new SliceModel(this);
    int index = m_slices.size();
    slice->setSliceIndex(index);
    m_slices.append(slice);

    if (!m_activeSlice) {
        m_activeSlice = slice;
        emit activeSliceChanged(0);
    }

    emit sliceAdded(index);
    return index;
}

void RadioModel::removeSlice(int index)
{
    if (index < 0 || index >= m_slices.size()) {
        return;
    }

    SliceModel* slice = m_slices.takeAt(index);
    if (m_activeSlice == slice) {
        m_activeSlice = m_slices.isEmpty() ? nullptr : m_slices.first();
        emit activeSliceChanged(m_activeSlice ? 0 : -1);
    }

    delete slice;
    emit sliceRemoved(index);
}

void RadioModel::setActiveSlice(int index)
{
    if (index >= 0 && index < m_slices.size()) {
        m_activeSlice = m_slices.at(index);
        emit activeSliceChanged(index);
    }
}

// --- Panadapter Management ---

int RadioModel::addPanadapter()
{
    auto* pan = new PanadapterModel(this);
    int index = m_panadapters.size();
    m_panadapters.append(pan);
    emit panadapterAdded(index);
    return index;
}

void RadioModel::removePanadapter(int index)
{
    if (index < 0 || index >= m_panadapters.size()) {
        return;
    }

    delete m_panadapters.takeAt(index);
    emit panadapterRemoved(index);
}

// --- Connection ---

void RadioModel::connectToRadio(const RadioInfo& info)
{
    // Tear down any existing connection
    if (m_connection) {
        teardownConnection();
    }

    m_lastRadioInfo = info;
    m_intentionalDisconnect = false;

    m_name = info.displayName();
    m_model = QString::fromLatin1(BoardCapsTable::forBoard(info.boardType).displayName);
    m_version = QString::number(info.firmwareVersion);
    emit infoChanged();

    // Configure ReceiverManager with hardware capabilities
    m_receiverManager->setMaxReceivers(info.maxReceivers);

    // Create receiver 0 with protocol-appropriate DDC mapping.
    // P2 2-ADC boards (ANAN-G2/Saturn) use DDC2 as primary RX per
    // Thetis console.cs:8216 UpdateDDCs. P1 radios deliver samples on
    // hardware receiver index 0, so leave the mapping auto-assigned
    // (which rebuildHardwareMapping resolves to 0 for the first active
    // receiver). Hardcoding DDC2 for everything dropped every P1 ep6
    // packet at ReceiverManager::feedIqData on tester hardware.
    int rxIdx = m_receiverManager->createReceiver();
    if (info.protocol == ProtocolVersion::Protocol2) {
        m_receiverManager->setDdcMapping(rxIdx, 2);   // DDC2 for 2-ADC P2 boards
    }
    m_receiverManager->setAdcForReceiver(rxIdx, 0); // ADC0

    // Create slice 0 and load persisted VFO state from AppSettings
    if (m_slices.isEmpty()) {
        addSlice();
    }
    setActiveSlice(0);
    m_activeSlice->setReceiverIndex(rxIdx);
    loadSliceState(m_activeSlice);

    // Activate receiver (this sends hardwareReceiverCountChanged to RadioConnection)
    m_receiverManager->activateReceiver(rxIdx);

    // Initialize WDSP DSP engine (wisdom runs async — channel creation
    // is deferred until initializedChanged fires)
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // Protocol-dependent DSP rates. Thetis P1 default is 192 kHz on the
    // wire (setup.cs: SampleRate1 default = 192000); Saturn/ANAN-G2 P2
    // streams at 768 kHz. in_size follows the Thetis formula
    // 64 * input_rate / 48000 so fexchange2 sees 48-kHz output chunks.
    const bool isP1 = (info.protocol == ProtocolVersion::Protocol1);
    const int wdspInputRate = isP1 ? 192000 : 768000;
    const int wdspInSize    = isP1 ? 256    : 1024;

    connect(m_wdspEngine, &WdspEngine::initializedChanged, this,
            [this, wdspInputRate, wdspInSize](bool ok) {
        if (!ok) {
            return;
        }
        // Create primary RX channel once WDSP is ready. in_size follows
        // Thetis cmaster.c create_rcvr: 64 * input_rate / 48000. WDSP
        // decimates input_rate -> 48000 internally and outputs 64 samples
        // per fexchange2 call.
        RxChannel* rxCh = m_wdspEngine->createRxChannel(0, wdspInSize, 4096,
                                                         wdspInputRate, 48000, 48000);
        if (rxCh) {
            // Apply slice state to WDSP channel (no longer hardcoded)
            if (m_activeSlice) {
                rxCh->setMode(m_activeSlice->dspMode());
                rxCh->setFilterFreqs(m_activeSlice->filterLow(),
                                     m_activeSlice->filterHigh());
                rxCh->setAgcMode(m_activeSlice->agcMode());
                rxCh->setAgcTop(m_activeSlice->rfGain());
            }
            rxCh->setActive(true);
        }
        // Apply volume from slice
        if (m_activeSlice) {
            m_audioEngine->setVolume(m_activeSlice->afGain() / 100.0f);
        }
        // Start audio output
        m_audioEngine->start();
        qCInfo(lcDsp) << "WDSP ready — RX channel 0 active, audio started";
    }, Qt::SingleShotConnection);
    m_wdspEngine->initialize(configDir);

    // Factory-create the connection (no parent — will be moved to thread)
    auto conn = RadioConnection::create(info);
    if (!conn) {
        qCWarning(lcConnection) << "Failed to create connection for" << info.displayName();
        return;
    }
    m_connection = conn.release();

    // Create worker thread
    m_connThread = new QThread(this);
    m_connThread->setObjectName(QStringLiteral("ConnectionThread"));

    // Move connection to worker thread BEFORE wiring signals
    m_connection->moveToThread(m_connThread);

    // Wire signals (auto-queued across threads)
    wireConnectionSignals();

    // Start thread — init() will be called on the worker thread
    connect(m_connThread, &QThread::started, m_connection, &RadioConnection::init);
    m_connThread->start();

    // CRITICAL: push sample rate + VFO frequency to the connection BEFORE
    // dispatching connectToRadio. The worker thread dequeues invokeMethod
    // calls in FIFO order, so whatever we queue first runs first. If we
    // queue connectToRadio before the setters, connectToRadio -> sendCommandFrame
    // -> composeEp2Frame reads m_rxFreqHz[0]=0 and m_sampleRate=48000 defaults
    // and sends a primed ep2 frame with phase word 0 to the radio just before
    // metis-start. Result: radio initializes DDC at freq=0 (bypass/idle state)
    // and streams ADC-pinned data with Q=0 forever. Verified against Thetis
    // NetworkIO.cs flow: Thetis always sets SetDDCRate + SetVFOfreq BEFORE
    // SendStartToMetis, so ForceCandCFrame inside SendStartToMetis reads the
    // correct freq/rate from globals.
    const int wireSampleRate = wdspInputRate;
    QMetaObject::invokeMethod(m_connection, [conn = m_connection, wireSampleRate]() {
        conn->setSampleRate(wireSampleRate);
    });
    if (m_activeSlice) {
        int hwRx = m_receiverManager->receiverConfig(0).hardwareRx;
        if (hwRx < 0) { hwRx = 0; }
        quint64 freqHz = m_activeSlice->frequency();
        QMetaObject::invokeMethod(m_connection, [conn = m_connection, hwRx, freqHz]() {
            conn->setReceiverFrequency(hwRx, freqHz);
        });
    }

    // Now dispatch connectToRadio -- it will find the correct m_rxFreqHz[0]
    // and m_sampleRate when sendCommandFrame runs inside it.
    QMetaObject::invokeMethod(m_connection, [conn = m_connection, info]() {
        conn->connectToRadio(info);
    });

    // Tell MainWindow / FFTEngine / SpectrumWidget the wire rate so bin math
    // matches (P1=192k, P2=768k). Without this the FFT thinks 768k and
    // compresses the real spectrum by 4x on P1.
    emit wireSampleRateChanged(static_cast<double>(wireSampleRate));

    qCDebug(lcConnection) << "Connecting to" << info.displayName()
                          << "P" << static_cast<int>(info.protocol);
}

void RadioModel::disconnectFromRadio()
{
    m_intentionalDisconnect = true;
    teardownConnection();
}

void RadioModel::wireConnectionSignals()
{
    if (!m_connection) {
        return;
    }

    // Connection state → RadioModel (auto-queued: connection thread → main thread)
    connect(m_connection, &RadioConnection::connectionStateChanged,
            this, &RadioModel::onConnectionStateChanged);

    // --- Slice → WDSP + RadioConnection ---
    // Wire active slice property changes to WDSP DSP engine and radio hardware.
    wireSliceSignals();

    // --- I/Q data → ReceiverManager → DSP worker → WDSP → AudioEngine ---
    // Route through ReceiverManager for DDC-aware mapping, then dispatch
    // to RxDspWorker on its own thread for fexchange2 processing.

    // Step 1: RadioConnection I/Q → ReceiverManager (DDC routing).
    // Auto connection: m_connection is on its worker thread, this is on
    // main, so the slot is queued onto the main thread.
    connect(m_connection, &RadioConnection::iqDataReceived,
            this, [this](int ddcIndex, const QVector<float>& samples) {
        m_receiverManager->feedIqData(ddcIndex, samples);
    });

    // Step 2a: ReceiverManager → spectrum fork (main thread, fast).
    // Kept on the main thread so rawIqData → FFTEngine routing stays
    // unchanged. FFTEngine lives on its own SpectrumThread and the
    // signal already crosses threads via queued connection.
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            this, [this](int receiverIndex, const QVector<float>& samples) {
        Q_UNUSED(receiverIndex);
        emit rawIqData(samples);
    });

    // Step 2b: ReceiverManager → DSP worker (queued, off the main thread).
    // RxDspWorker accumulates samples into in_size chunks, runs each
    // chunk through RxChannel::processIq → fexchange2, then forwards
    // decoded audio to AudioEngine. fexchange2 must NOT run on the
    // main/GUI thread — see RxDspWorker.h for the deadlock rationale.
    Q_ASSERT(m_dspThread == nullptr && m_dspWorker == nullptr);
    m_dspThread = new QThread(this);
    m_dspThread->setObjectName(QStringLiteral("DspThread"));
    m_dspWorker = new RxDspWorker();   // no parent — moved to thread
    m_dspWorker->setEngines(m_wdspEngine, m_audioEngine);
    m_dspWorker->moveToThread(m_dspThread);
    connect(m_dspThread, &QThread::finished,
            m_dspWorker, &QObject::deleteLater);
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            m_dspWorker, &RxDspWorker::processIqBatch,
            Qt::QueuedConnection);
    m_dspThread->start();

    // Meter data → MeterModel
    connect(m_connection, &RadioConnection::meterDataReceived,
            this, [this](float fwd, float rev, float voltage, float current) {
        Q_UNUSED(voltage);
        Q_UNUSED(current);
        Q_UNUSED(fwd);
        Q_UNUSED(rev);
    });

    // Error handling
    connect(m_connection, &RadioConnection::errorOccurred,
            this, [this](NereusSDR::RadioConnectionError code, const QString& msg) {
        Q_UNUSED(code);
        qCWarning(lcConnection) << "Connection error:" << msg;
    });

    // ReceiverManager → RadioConnection (hardware updates)
    connect(m_receiverManager, &ReceiverManager::hardwareReceiverCountChanged,
            this, [this](int count) {
        if (m_connection) {
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, count]() {
                conn->setActiveReceiverCount(count);
            });
        }
    });

    connect(m_receiverManager, &ReceiverManager::hardwareFrequencyChanged,
            this, [this](int hwIndex, quint64 freq) {
        if (m_connection) {
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, hwIndex, freq]() {
                conn->setReceiverFrequency(hwIndex, freq);
            });
        }
    });
}

// Wire active slice signals to WDSP channel and radio hardware.
// Called from wireConnectionSignals after connection is established.
void RadioModel::wireSliceSignals()
{
    if (!m_activeSlice || !m_connection) {
        return;
    }

    SliceModel* slice = m_activeSlice;

    // Frequency → ReceiverManager → radio hardware
    // ReceiverManager handles DDC mapping (receiver 0 → DDC2 for ANAN-G2)
    connect(slice, &SliceModel::frequencyChanged, this, [this, slice](double freq) {
        int rxIdx = slice->receiverIndex();
        if (rxIdx >= 0) {
            m_receiverManager->setReceiverFrequency(rxIdx, static_cast<quint64>(freq));
        }
        // TX follows RX (simplex)
        if (m_connection) {
            quint64 freqHz = static_cast<quint64>(freq);
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, freqHz]() {
                conn->setTxFrequency(freqHz);
            });
        }
        scheduleSettingsSave();
    });

    // Mode → WDSP
    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setMode(mode);
        }
        scheduleSettingsSave();
    });

    // Filter → WDSP
    connect(slice, &SliceModel::filterChanged, this, [this](int low, int high) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFilterFreqs(low, high);
        }
        scheduleSettingsSave();
    });

    // AGC → WDSP
    connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcMode(mode);
        }
        scheduleSettingsSave();
    });

    // AF gain → AudioEngine volume
    connect(slice, &SliceModel::afGainChanged, this, [this](int gain) {
        m_audioEngine->setVolume(gain / 100.0f);
        scheduleSettingsSave();
    });

    // RF gain → WDSP AGC top
    connect(slice, &SliceModel::rfGainChanged, this, [this](int gain) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcTop(static_cast<double>(gain));
        }
        scheduleSettingsSave();
    });

    // Antenna changes → Alex register via RadioConnection
    connect(slice, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        if (m_connection) {
            // Map antenna name to index: ANT1=0, ANT2=1, ANT3=2
            int idx = 0;
            if (ant == QLatin1String("ANT2")) { idx = 1; }
            else if (ant == QLatin1String("ANT3")) { idx = 2; }
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, idx]() {
                conn->setAntenna(idx);
            });
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::txAntennaChanged, this, [this](const QString&) {
        // TX antenna is set in the same setAntenna call for now
        // Full TX-specific antenna routing deferred to TX implementation
        scheduleSettingsSave();
    });

    // Send initial frequency to radio (after connection init completes)
    QTimer::singleShot(100, this, [this, slice]() {
        if (m_connection && m_connection->isConnected()) {
            int rxIdx = slice->receiverIndex();
            quint64 freqHz = static_cast<quint64>(slice->frequency());
            if (rxIdx >= 0) {
                m_receiverManager->setReceiverFrequency(rxIdx, freqHz);
            }
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, freqHz]() {
                conn->setTxFrequency(freqHz);
            });
        }
    });
}

// Load persisted VFO state from AppSettings into a slice.
void RadioModel::loadSliceState(SliceModel* slice)
{
    if (!slice) {
        return;
    }

    auto& s = AppSettings::instance();

    double freq = s.value(QStringLiteral("VfoFrequency"), 14225000.0).toDouble();
    slice->setFrequency(freq);

    int modeInt = s.value(QStringLiteral("VfoDspMode"),
                          static_cast<int>(DSPMode::USB)).toInt();
    DSPMode mode = static_cast<DSPMode>(modeInt);
    // Set mode without auto-applying default filter (load persisted filter instead)
    // We need to set the mode field directly and then load filter separately
    slice->setDspMode(mode);

    // Override filter with persisted values if they exist
    if (s.contains(QStringLiteral("VfoFilterLow")) &&
        s.contains(QStringLiteral("VfoFilterHigh"))) {
        int low = s.value(QStringLiteral("VfoFilterLow")).toInt();
        int high = s.value(QStringLiteral("VfoFilterHigh")).toInt();
        slice->setFilter(low, high);
    }

    int agcInt = s.value(QStringLiteral("VfoAgcMode"),
                         static_cast<int>(AGCMode::Med)).toInt();
    slice->setAgcMode(static_cast<AGCMode>(agcInt));

    int stepHz = s.value(QStringLiteral("VfoStepHz"), 100).toInt();
    slice->setStepHz(stepHz);

    int afGain = s.value(QStringLiteral("VfoAfGain"), 50).toInt();
    slice->setAfGain(afGain);

    int rfGain = s.value(QStringLiteral("VfoRfGain"), 80).toInt();
    slice->setRfGain(rfGain);

    QString rxAnt = s.value(QStringLiteral("VfoRxAntenna"),
                            QStringLiteral("ANT1")).toString();
    slice->setRxAntenna(rxAnt);

    QString txAnt = s.value(QStringLiteral("VfoTxAntenna"),
                            QStringLiteral("ANT1")).toString();
    slice->setTxAntenna(txAnt);

    qCInfo(lcDsp) << "Loaded VFO state:"
                  << SliceModel::modeName(mode)
                  << freq / 1e6 << "MHz"
                  << "AGC:" << agcInt
                  << "AF:" << afGain << "RF:" << rfGain;
}

// Coalesce settings saves to avoid writing on every scroll tick.
void RadioModel::scheduleSettingsSave()
{
    if (m_settingsSaveScheduled) {
        return;
    }
    m_settingsSaveScheduled = true;
    QTimer::singleShot(500, this, [this]() {
        m_settingsSaveScheduled = false;
        saveSliceState(m_activeSlice);
    });
}

// Persist current slice state to AppSettings.
void RadioModel::saveSliceState(SliceModel* slice)
{
    if (!slice) {
        return;
    }

    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("VfoFrequency"), slice->frequency());
    s.setValue(QStringLiteral("VfoDspMode"), static_cast<int>(slice->dspMode()));
    s.setValue(QStringLiteral("VfoFilterLow"), slice->filterLow());
    s.setValue(QStringLiteral("VfoFilterHigh"), slice->filterHigh());
    s.setValue(QStringLiteral("VfoAgcMode"), static_cast<int>(slice->agcMode()));
    s.setValue(QStringLiteral("VfoStepHz"), slice->stepHz());
    s.setValue(QStringLiteral("VfoAfGain"), slice->afGain());
    s.setValue(QStringLiteral("VfoRfGain"), slice->rfGain());
    s.setValue(QStringLiteral("VfoRxAntenna"), slice->rxAntenna());
    s.setValue(QStringLiteral("VfoTxAntenna"), slice->txAntenna());
}

void RadioModel::teardownConnection()
{
    if (!m_connection) {
        return;
    }

    // Disconnect signals into the DSP worker first so no new I/Q
    // batches can be posted onto the worker thread, then quit and
    // join that thread before touching WDSP. The worker is queued
    // for deletion via QThread::finished (see wireConnectionSignals),
    // so the m_dspWorker pointer may dangle after wait() returns —
    // null it out to avoid a use-after-free in any later teardown.
    if (m_dspWorker != nullptr) {
        QObject::disconnect(m_receiverManager, nullptr, m_dspWorker, nullptr);
    }
    if (m_dspThread != nullptr) {
        m_dspThread->quit();
        m_dspThread->wait();
        delete m_dspThread;
        m_dspThread = nullptr;
        m_dspWorker = nullptr;
    }

    // Stop audio output
    m_audioEngine->stop();

    // Shutdown WDSP (destroys all channels, saves cache)
    m_wdspEngine->shutdown();

    // Disconnect remaining signals (prevents new work being queued)
    QObject::disconnect(m_connection, nullptr, this, nullptr);
    QObject::disconnect(m_connection, nullptr, m_receiverManager, nullptr);
    QObject::disconnect(m_receiverManager, nullptr, this, nullptr);

    // Tear down the connection on its own worker thread via the shared
    // helper. See src/core/RadioConnectionTeardown.h for why this must
    // run on the worker — short version: the RadioConnection's QTimers
    // are thread-affined to the worker and destroying them on any other
    // thread emits cross-thread warnings and can crash on Windows.
    teardownWorkerThreadedConnection(m_connection, m_connThread);
}

// Phase 3G-9b — 7 smooth-default recipe values. See docs/architecture/waterfall-tuning.md.
void RadioModel::applyClaritySmoothDefaults()
{
    SpectrumWidget* sw = spectrumWidget();
    if (!sw) { return; }  // not yet wired by MainWindow — Task 3 re-invokes

    // 1. Palette — narrow-band monochrome. See docs/architecture/waterfall-tuning.md §1.
    sw->setWfColorScheme(WfColorScheme::ClarityBlue);

    // 2. Spectrum averaging mode — log-recursive for heavy smoothing.
    sw->setAverageMode(AverageMode::Logarithmic);

    // 3. Averaging alpha — very slow exponential (~500 ms perceived smoothing
    //    at 30 FPS). See waterfall-tuning.md §3.
    sw->setAverageAlpha(0.05f);

    // 4. Trace colour — pure white, thin, sits cleanly in front of the
    //    waterfall without competing. Visual target: 2026-04-14 reference.
    sw->setFillColor(QColor(0xff, 0xff, 0xff, 230));

    // 5. Pan fill OFF — trace renders as a thin line, not a filled curve.
    //    NereusSDR's default is fill-on; turn it off to match the reference.
    sw->setPanFillEnabled(false);

    // 6. Waterfall AGC — tracks band conditions automatically. With AGC on,
    //    the Low/High threshold values are continuously overwritten by the
    //    running min/max of visible FFT bins (see SpectrumWidget.cpp
    //    pushWaterfallRow), so setting them in this profile is redundant.
    sw->setWfAgcEnabled(true);

    // 7. Waterfall update period — 30 ms for smooth scroll motion.
    sw->setWfUpdatePeriodMs(30);

    // Mark the profile as applied so the gate short-circuits on next launch.
    AppSettings::instance().setValue(
        QStringLiteral("DisplayProfileApplied"),
        QStringLiteral("True"));
}

void RadioModel::onConnectionStateChanged(ConnectionState state)
{
    emit connectionStateChanged();

    switch (state) {
    case ConnectionState::Connected:
        qCDebug(lcConnection) << "Connected to" << m_name;
        // Phase 3I Task 17 — record the most recently used radio so
        // tryAutoReconnect() targets the right entry on next launch.
        if (!m_lastRadioInfo.macAddress.isEmpty()) {
            AppSettings& s = AppSettings::instance();
            s.setLastConnected(m_lastRadioInfo.macAddress);
            s.save();
            // Exempt this MAC from discovery stale-removal — once the
            // radio is streaming it stops replying to broadcasts.
            m_discovery->setConnectedMac(m_lastRadioInfo.macAddress);
        }
        // Phase 3I — fan out to HardwarePage so its sub-tabs populate with
        // the connected radio's fields (Radio Info labels, sample rate,
        // capability-gated tab visibility, per-MAC settings restore).
        emit currentRadioChanged(m_lastRadioInfo);
        break;
    case ConnectionState::Disconnected:
        qCDebug(lcConnection) << "Disconnected from" << m_name;
        m_discovery->clearConnectedMac();
        break;
    case ConnectionState::Connecting:
        qCDebug(lcConnection) << "Connecting to" << m_name << "...";
        break;
    case ConnectionState::Error:
        qCWarning(lcConnection) << "Connection error for" << m_name;
        m_discovery->clearConnectedMac();
        break;
    }
}

} // namespace NereusSDR
