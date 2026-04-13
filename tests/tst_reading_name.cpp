// tst_reading_name.cpp — readingName(bindingId) maps MeterBinding constants
// to the Thetis-spelled labels rendered by ScaleItem::ShowType.
//
// Source of truth: Thetis MeterManager.cs:2258-2318 (ReadingName() switch).

#include <QtTest/QtTest>

#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding namespace

using namespace NereusSDR;

class TstReadingName : public QObject
{
    Q_OBJECT

private slots:
    void rx_bindings_match_thetis()
    {
        QCOMPARE(readingName(MeterBinding::SignalPeak),   QStringLiteral("Signal Peak"));
        QCOMPARE(readingName(MeterBinding::SignalAvg),    QStringLiteral("Signal Average"));
        QCOMPARE(readingName(MeterBinding::AdcPeak),      QStringLiteral("ADC Peak"));
        QCOMPARE(readingName(MeterBinding::AdcAvg),       QStringLiteral("ADC Average"));
        QCOMPARE(readingName(MeterBinding::AgcGain),      QStringLiteral("AGC Gain"));
        QCOMPARE(readingName(MeterBinding::AgcPeak),      QStringLiteral("AGC Peak"));
        QCOMPARE(readingName(MeterBinding::AgcAvg),       QStringLiteral("AGC Average"));
        QCOMPARE(readingName(MeterBinding::SignalMaxBin), QStringLiteral("Signal Max FFT Bin"));
        QCOMPARE(readingName(MeterBinding::PbSnr),        QStringLiteral("Estimated PBSNR"));
    }

    void tx_bindings_match_thetis()
    {
        QCOMPARE(readingName(MeterBinding::TxPower),        QStringLiteral("Power"));
        QCOMPARE(readingName(MeterBinding::TxReversePower), QStringLiteral("Reverse Power"));
        QCOMPARE(readingName(MeterBinding::TxSwr),          QStringLiteral("SWR"));
        QCOMPARE(readingName(MeterBinding::TxMic),          QStringLiteral("MIC"));
        QCOMPARE(readingName(MeterBinding::TxComp),         QStringLiteral("Compression"));
        QCOMPARE(readingName(MeterBinding::TxAlc),          QStringLiteral("ALC"));
        QCOMPARE(readingName(MeterBinding::TxEq),           QStringLiteral("EQ"));
        QCOMPARE(readingName(MeterBinding::TxLeveler),      QStringLiteral("Leveler"));
        QCOMPARE(readingName(MeterBinding::TxLevelerGain),  QStringLiteral("Leveler Gain"));
        QCOMPARE(readingName(MeterBinding::TxAlcGain),      QStringLiteral("ALC Gain"));
        QCOMPARE(readingName(MeterBinding::TxAlcGroup),     QStringLiteral("ALC Group"));
        QCOMPARE(readingName(MeterBinding::TxCfc),          QStringLiteral("CFC Compression Average"));
        QCOMPARE(readingName(MeterBinding::TxCfcGain),      QStringLiteral("CFC Compression"));
    }

    void hardware_and_rotator_bindings()
    {
        QCOMPARE(readingName(MeterBinding::HwVolts),  QStringLiteral("Volts"));
        QCOMPARE(readingName(MeterBinding::HwAmps),   QStringLiteral("Amps"));
        QCOMPARE(readingName(MeterBinding::RotatorAz), QStringLiteral("Azimuth"));
        QCOMPARE(readingName(MeterBinding::RotatorEle),QStringLiteral("Elevation"));
    }

    void unknown_binding_returns_empty()
    {
        // Thetis falls through to reading.ToString() for unmapped values.
        // NereusSDR returns an empty string instead — the render pass
        // skips empty titles.
        QCOMPARE(readingName(-1),    QString());
        QCOMPARE(readingName(99999), QString());
    }
};

QTEST_MAIN(TstReadingName)
#include "tst_reading_name.moc"
