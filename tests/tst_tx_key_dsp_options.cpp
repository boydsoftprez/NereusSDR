// =================================================================
// tests/tst_tx_key_dsp_options.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. No upstream logic is ported here.
//
// R-IOS-13 (2026-09-27): whichever way the transmitter is keyed (MOX, TUNE,
// a remote device's key, VOX or the two-tone test), the TX-bound slice's
// DSP > Options reach the TX channel before its first block. The Core opens
// the TX channel at dsp 2048 (WdspEngine::kTxDspBufferSize); until the
// Phone TX options (64 / 4096 / Low Latency) apply, a microphone sample
// takes 65.6 ms through TX DSP instead of 16.1 ms (tst_tx_latency_dsp).
//
// A local RadioModel with a mock connection and a real WDSP TX channel
// (no radio, no RF, no audio device). The check is made when the hardware
// flips to transmit, which is before the TX channel starts.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original test for NereusSDR by J.J. Boyd (KG4VCF),
//               R-IOS-13, with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

} // namespace

class TestTxKeyDspOptions : public QObject {
    Q_OBJECT

    // A model on 20 m USB with the Phone TX options at their defaults, a
    // real TX channel at the Core's open sizes, two-tone ready to run.
    struct Rig {
        RadioModel model;
        MockConnection conn;
        WdspEngine* engine{nullptr};
        TxChannel* tx{nullptr};
        int blockSizeAtFlip{-1};
        int flips{0};

        bool build()
        {
            AppSettings::instance().clear();
            model.setCapsForTest(/*hasAlex=*/false);
            model.injectConnectionForTest(&conn);
            model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
            model.setTuneOffSettleMsForTest(0);
            model.addSlice();
            if (SliceModel* slice = model.activeSlice()) {
                slice->setDspMode(DSPMode::USB);
                slice->setFrequency(14'200'000.0);
            }
            engine = model.wdspEngine();
            engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
            tx = engine->createTxChannel(WdspEngine::kTxChannelId, 64,
                                         WdspEngine::kTxDspBufferSize, 48000,
                                         WdspEngine::kTxDspSampleRate, 192000);
            if (tx == nullptr || !model.waitForTransmitLaneForTest(600000)) {
                return false;
            }
            tx->setConnection(&conn);
            tx->setWdspEngine(engine);
            model.injectTxChannelForTest(tx);
            TwoToneController* twoTone = model.twoToneController();
            twoTone->setTxChannel(tx);
            twoTone->setPowerOn(true);
            twoTone->setSettleDelaysMs(0, 0);
            QObject::connect(model.moxController(), &MoxController::hardwareFlipped,
                             &model, [this](bool isTx) {
                if (isTx) {
                    ++flips;
                    blockSizeAtFlip = tx->txDspBlockSize();
                }
            });
            return true;
        }
        ~Rig()
        {
            if (tx != nullptr) {
                model.twoToneController()->setTxChannel(nullptr);
                tx->setConnection(nullptr);
                model.injectTxChannelForTest(nullptr);
                engine->shutdown();
            }
            model.injectConnectionForTest(nullptr);
            AppSettings::instance().clear();
        }
    };

    static void pump(int passes = 20)
    {
        for (int i = 0; i < passes; ++i) {
            QCoreApplication::processEvents();
        }
    }

private slots:
    void everyKeyPathAppliesTheModesTxOptionsFirst_data();
    void everyKeyPathAppliesTheModesTxOptionsFirst();
};

void TestTxKeyDspOptions::everyKeyPathAppliesTheModesTxOptionsFirst_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("MOX") << QStringLiteral("mox");
    QTest::newRow("TUNE") << QStringLiteral("tune");
    QTest::newRow("remote key") << QStringLiteral("remote");
    QTest::newRow("VOX") << QStringLiteral("vox");
    QTest::newRow("two-tone") << QStringLiteral("twoTone");
}

void TestTxKeyDspOptions::everyKeyPathAppliesTheModesTxOptionsFirst()
{
    QFETCH(QString, path);
    Rig rig;
    QVERIFY(rig.build());
    // The channel is still at the sizes the Core opens it with.
    QCOMPARE(rig.tx->txDspBlockSize(), WdspEngine::kTxDspBufferSize);
    QList<DSPMode> applied;
    rig.model.setTxKeyDspOptionsObserverForTest([&applied](DSPMode mode) {
        applied.append(mode);
    });

    MoxController* mox = rig.model.moxController();
    if (path == QLatin1String("mox")) {
        mox->setMox(true);
    } else if (path == QLatin1String("tune")) {
        rig.model.setTune(true);
    } else if (path == QLatin1String("remote")) {
        KeyerIdentity keyer;
        keyer.deviceId = QByteArrayLiteral("phone-1");
        keyer.session = QStringLiteral("station:1");
        mox->setMox(true, keyer);
    } else if (path == QLatin1String("vox")) {
        mox->setVoxEnabled(true);
        mox->onVoxActive(true);
    } else {
        rig.model.setTwoTone(true);
    }
    QTRY_VERIFY_WITH_TIMEOUT(rig.flips > 0, 5000);
    QVERIFY2(mox->isMox(), "the path did not key");
    // Applied before the hardware flip, so before the channel's first block.
    QCOMPARE(rig.blockSizeAtFlip, 64);
    QVERIFY(!applied.isEmpty());
    QCOMPARE(applied.first(), DSPMode::USB);
    QVERIFY(rig.model.waitForTransmitLaneForTest(600000));

    if (path == QLatin1String("tune")) {
        rig.model.setTune(false);
    } else if (path == QLatin1String("twoTone")) {
        rig.model.setTwoTone(false);
    } else if (path == QLatin1String("vox")) {
        mox->onVoxActive(false);
    } else {
        mox->setMox(false);
    }
    QTRY_VERIFY_WITH_TIMEOUT(!mox->isMox(), 5000);
    pump();
    QVERIFY(rig.model.waitForTransmitLaneForTest(600000));
}

QTEST_MAIN(TestTxKeyDspOptions)
#include "tst_tx_key_dsp_options.moc"
