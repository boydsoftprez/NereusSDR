// =================================================================
// tests/tst_tx_meter_index.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// D14, R-R3-49: every NereusSDR transmit meter reads the WDSP meter it
// names. NereusSDR's TxMeterType values do not follow WDSP's txaMeterType
// order, so wdspTxaMeterIndex maps each one to the GetTXAMeter index Thetis
// reads for it (Console/dsp.cs CalculateTXMeter [v2.10.3.15]).
//
// The expected indices are written out by hand from the enum this build
// compiles, third_party/wdsp/src/TXA.h (txaMeterType, lines 49-69):
//   TXA_MIC_PK 0, TXA_MIC_AV 1, TXA_EQ_PK 2, TXA_EQ_AV 3,
//   TXA_LVLR_PK 4, TXA_LVLR_AV 5, TXA_LVLR_GAIN 6,
//   TXA_CFC_PK 7, TXA_CFC_AV 8, TXA_CFC_GAIN 9,
//   TXA_COMP_PK 10, TXA_COMP_AV 11,
//   TXA_ALC_PK 12, TXA_ALC_AV 13, TXA_ALC_GAIN 14,
//   TXA_OUT_PK 15, TXA_OUT_AV 16.
// =================================================================
#include <QtTest/QtTest>

#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestTxMeterIndex : public QObject {
    Q_OBJECT

private slots:
    void eachMeterReadsTheWdspMeterItNames_data()
    {
        QTest::addColumn<int>("meter");
        QTest::addColumn<int>("wdspIndex");
        auto row = [](const char* name, TxMeterType meter, int index) {
            QTest::newRow(name) << static_cast<int>(meter) << index;
        };
        row("MIC peak",       TxMeterType::MicPeak,      0);
        row("MIC average",    TxMeterType::MicAvg,       1);
        row("EQ peak",        TxMeterType::EqPeak,       2);
        row("EQ average",     TxMeterType::EqAvg,        3);
        row("LEV peak",       TxMeterType::LevelerPeak,  4);
        row("LEV average",    TxMeterType::LevelerAvg,   5);
        row("LEV gain",       TxMeterType::LevelerGain,  6);
        row("CFC peak",       TxMeterType::CfcPeak,      7);
        row("CFC average",    TxMeterType::CfcAvg,       8);
        row("CFC gain",       TxMeterType::CfcGain,      9);
        row("COMP peak",      TxMeterType::CompPeak,    10);
        row("COMP average",   TxMeterType::CompAvg,     11);
        row("ALC peak",       TxMeterType::AlcPeak,     12);
        row("ALC average",    TxMeterType::AlcAvg,      13);
        row("ALC gain",       TxMeterType::AlcGain,     14);
        row("OUT peak",       TxMeterType::OutPeak,     15);
        row("OUT average",    TxMeterType::OutAvg,      16);
    }

    void eachMeterReadsTheWdspMeterItNames()
    {
        QFETCH(int, meter);
        QFETCH(int, wdspIndex);
        QCOMPARE(wdspTxaMeterIndex(static_cast<TxMeterType>(meter)), wdspIndex);
    }

    // Seventeen meters, seventeen distinct indices: no two NereusSDR meters
    // read the same WDSP meter.
    void noTwoMetersShareAnIndex()
    {
        QSet<int> seen;
        for (int m = 0; m <= static_cast<int>(TxMeterType::LevelerGain); ++m) {
            const int index = wdspTxaMeterIndex(static_cast<TxMeterType>(m));
            QVERIFY(index >= 0 && index < 17);
            QVERIFY2(!seen.contains(index), qPrintable(QString::number(index)));
            seen.insert(index);
        }
        QCOMPARE(seen.size(), 17);
    }
};

QTEST_GUILESS_MAIN(TestTxMeterIndex)
#include "tst_tx_meter_index.moc"
