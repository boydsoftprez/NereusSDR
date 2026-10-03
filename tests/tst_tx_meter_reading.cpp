// =================================================================
// tests/tst_tx_meter_reading.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// D14, R-R3-49: every transmit meter NereusSDR shows reads exactly what
// Thetis shows for the same WDSP reading. Thetis's reading is two steps:
//
//   1. Console/dsp.cs CalculateTXMeter [v2.10.3.15] reads one GetTXAMeter
//      index per meter type, adds alcgain (3.0) to ALC_G, and returns the
//      value negated: -(float)val.
//   2. Console/console.cs:46969-46986 [v2.10.3.15] negates it again for most
//      readings and floors it: MIC and ALC_PK / ALC_G at -195, the others at
//      -30; LVL_G is max(0, CalculateTXMeter(LVL_G)) (one negation only),
//      CFC_G max(0, -CalculateTXMeter(CFC_G)); ALC_GROUP is
//      max(-30, ALC_PK) + max(0, ALC_G).
//
// Worked by hand from those two steps, per NereusSDR meter binding:
//   TxMic         max(-195, MIC_AV)
//   TxEq          max(-30,  EQ_AV)
//   TxLeveler     max(-30,  LVLR_AV)
//   TxLevelerGain max(0,   -LVLR_GAIN)
//   TxCfc         max(-30,  CFC_AV)
//   TxCfcGain     max(0,    CFC_GAIN)
//   TxComp        max(-30,  COMP_AV)
//   TxAlc         max(-30,  ALC_AV)
//   TxAlcGain     max(-195, ALC_GAIN + 3)
//   TxAlcGroup    max(-30,  ALC_PK) + max(0, ALC_GAIN + 3)
// Each WDSP meter below holds a different value, so a meter that read the
// wrong index would show a wrong number.
// =================================================================
#include <QtTest/QtTest>

#include <array>

#include "core/WdspTypes.h"
#include "gui/meters/MeterPoller.h"

using namespace NereusSDR;

namespace {

constexpr int kTxaMeters = 17;
using Raw = std::array<double, kTxaMeters>;   // indexed by TxMeterType value

Raw rawTable(double micAv, double micPk, double eqAv, double eqPk,
             double lvlrAv, double lvlrPk, double lvlrGain,
             double cfcAv, double cfcPk, double cfcGain,
             double compAv, double compPk,
             double alcAv, double alcPk, double alcGain,
             double outPk, double outAv)
{
    Raw r{};
    r[static_cast<int>(TxMeterType::MicAvg)] = micAv;
    r[static_cast<int>(TxMeterType::MicPeak)] = micPk;
    r[static_cast<int>(TxMeterType::EqAvg)] = eqAv;
    r[static_cast<int>(TxMeterType::EqPeak)] = eqPk;
    r[static_cast<int>(TxMeterType::LevelerAvg)] = lvlrAv;
    r[static_cast<int>(TxMeterType::LevelerPeak)] = lvlrPk;
    r[static_cast<int>(TxMeterType::LevelerGain)] = lvlrGain;
    r[static_cast<int>(TxMeterType::CfcAvg)] = cfcAv;
    r[static_cast<int>(TxMeterType::CfcPeak)] = cfcPk;
    r[static_cast<int>(TxMeterType::CfcGain)] = cfcGain;
    r[static_cast<int>(TxMeterType::CompAvg)] = compAv;
    r[static_cast<int>(TxMeterType::CompPeak)] = compPk;
    r[static_cast<int>(TxMeterType::AlcAvg)] = alcAv;
    r[static_cast<int>(TxMeterType::AlcPeak)] = alcPk;
    r[static_cast<int>(TxMeterType::AlcGain)] = alcGain;
    r[static_cast<int>(TxMeterType::OutPeak)] = outPk;
    r[static_cast<int>(TxMeterType::OutAvg)] = outAv;
    return r;
}

// A transmitting station: every stage running.
const Raw kLive = rawTable(-12.5, -8.0, -14.0, -9.0,
                           -16.0, -11.0, 6.0,
                           -18.0, -13.0, 4.5,
                           -20.0, -15.0,
                           -22.0, -17.0, -2.0,
                           -3.0, -6.0);

// Every stage off: WDSP's meter.c xmeter writes -400 for average and peak
// and +0.0 for gain.
const Raw kOff = rawTable(-400.0, -400.0, -400.0, -400.0,
                          -400.0, -400.0, 0.0,
                          -400.0, -400.0, 0.0,
                          -400.0, -400.0,
                          -400.0, -400.0, 0.0,
                          -400.0, -400.0);

// Past the floors, and gains of the other sign.
const Raw kDeep = rawTable(-300.0, -250.0, -45.0, -31.0,
                           -30.5, -29.5, -4.0,
                           -60.0, -2.0, -7.0,
                           -30.0, -1.0,
                           -35.0, -40.0, -10.0,
                           -1.0, -2.0);

} // namespace

class TestTxMeterReading : public QObject {
    Q_OBJECT

private slots:
    void eachMeterShowsWhatThetisShows_data()
    {
        QTest::addColumn<int>("binding");
        QTest::addColumn<int>("table");   // 0 live, 1 off, 2 deep
        QTest::addColumn<double>("expected");

        using namespace MeterBinding;
        // Live: every stage running.
        QTest::newRow("live mic")         << TxMic         << 0 << -12.5;
        QTest::newRow("live eq")          << TxEq          << 0 << -14.0;
        QTest::newRow("live leveler")     << TxLeveler     << 0 << -16.0;
        QTest::newRow("live lvl gain")    << TxLevelerGain << 0 << 0.0;    // max(0, -6)
        QTest::newRow("live cfc")         << TxCfc         << 0 << -18.0;
        QTest::newRow("live cfc gain")    << TxCfcGain     << 0 << 4.5;
        QTest::newRow("live comp")        << TxComp        << 0 << -20.0;
        QTest::newRow("live alc")         << TxAlc         << 0 << -22.0;
        QTest::newRow("live alc gain")    << TxAlcGain     << 0 << 1.0;    // -2 + 3
        QTest::newRow("live alc group")   << TxAlcGroup    << 0 << -16.0;  // -17 + 1
        // Off: every stage off.
        QTest::newRow("off mic")          << TxMic         << 1 << -195.0;
        QTest::newRow("off eq")           << TxEq          << 1 << -30.0;
        QTest::newRow("off leveler")      << TxLeveler     << 1 << -30.0;
        QTest::newRow("off lvl gain")     << TxLevelerGain << 1 << 0.0;
        QTest::newRow("off cfc")          << TxCfc         << 1 << -30.0;
        QTest::newRow("off cfc gain")     << TxCfcGain     << 1 << 0.0;
        QTest::newRow("off comp")         << TxComp        << 1 << -30.0;
        QTest::newRow("off alc")          << TxAlc         << 1 << -30.0;
        QTest::newRow("off alc gain")     << TxAlcGain     << 1 << 3.0;    // 0 + 3
        QTest::newRow("off alc group")    << TxAlcGroup    << 1 << -27.0;  // -30 + 3
        // Deep: past the floors, gains of the other sign.
        QTest::newRow("deep mic")         << TxMic         << 2 << -195.0;
        QTest::newRow("deep eq")          << TxEq          << 2 << -30.0;
        QTest::newRow("deep leveler")     << TxLeveler     << 2 << -30.0;  // -30.5 floored
        QTest::newRow("deep lvl gain")    << TxLevelerGain << 2 << 4.0;    // max(0, 4)
        QTest::newRow("deep cfc")         << TxCfc         << 2 << -30.0;
        QTest::newRow("deep cfc gain")    << TxCfcGain     << 2 << 0.0;    // max(0, -7)
        QTest::newRow("deep comp")        << TxComp        << 2 << -30.0;
        QTest::newRow("deep alc")         << TxAlc         << 2 << -30.0;
        QTest::newRow("deep alc gain")    << TxAlcGain     << 2 << -7.0;   // -10 + 3
        QTest::newRow("deep alc group")   << TxAlcGroup    << 2 << -30.0;  // -30 + max(0, -7)
    }

    void eachMeterShowsWhatThetisShows()
    {
        QFETCH(int, binding);
        QFETCH(int, table);
        QFETCH(double, expected);
        const Raw& raw = table == 0 ? kLive : (table == 1 ? kOff : kDeep);
        const double shown = MeterPoller::txReadingForBinding(
            binding, [&raw](TxMeterType m) { return raw[static_cast<int>(m)]; });
        QCOMPARE(shown, expected);
    }

    // CalculateTXMeter itself: the index each Thetis meter type reads, the
    // ALC gain offset and the sign flip.
    void calculateTxMeterReadsNegatesAndOffsets()
    {
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Mic), TxMeterType::MicAvg);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Pwr), TxMeterType::OutPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Alc), TxMeterType::AlcAvg);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Eq), TxMeterType::EqAvg);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Leveler), TxMeterType::LevelerAvg);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Comp), TxMeterType::CompAvg);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::Cpdr), TxMeterType::CompAvg);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::AlcG), TxMeterType::AlcGain);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::LvlG), TxMeterType::LevelerGain);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::MicPk), TxMeterType::MicPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::AlcPk), TxMeterType::AlcPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::EqPk), TxMeterType::EqPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::LevelerPk), TxMeterType::LevelerPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::CompPk), TxMeterType::CompPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::CpdrPk), TxMeterType::CompPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::CfcPk), TxMeterType::CfcPeak);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::CfcG), TxMeterType::CfcGain);
        QCOMPARE(calculateTxMeterSource(ThetisTxMeterType::CfcAv), TxMeterType::CfcAvg);

        QCOMPARE(calculateTxMeter(ThetisTxMeterType::Alc, -22.0), 22.0f);
        QCOMPARE(calculateTxMeter(ThetisTxMeterType::AlcG, -2.0), -1.0f);   // -(-2 + 3)
        QCOMPARE(calculateTxMeter(ThetisTxMeterType::LvlG, 6.0), -6.0f);
        QCOMPARE(calculateTxMeter(ThetisTxMeterType::Pwr, -3.0), 3.0f);
    }

    // The TCI transmit sensors' mic level is Thetis's MIC reading.
    void tciMicLevelIsTheMicReading()
    {
        QCOMPARE(thetisTxReading(ThetisTxReading::Mic,
                                 [](TxMeterType m) {
                                     return m == TxMeterType::MicAvg ? -12.5 : 99.0;
                                 }),
                 -12.5f);
        QCOMPARE(thetisTxReading(ThetisTxReading::Mic, [](TxMeterType) { return -400.0; }),
                 -195.0f);
    }
};

QTEST_MAIN(TestTxMeterReading)
#include "tst_tx_meter_reading.moc"
