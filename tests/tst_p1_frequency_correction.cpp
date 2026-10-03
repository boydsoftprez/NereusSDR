// no-port-check: NereusSDR-original test of the Protocol 1 frequency
// correction ported from Thetis NetworkIO.VFOfreq (R-R3-49).
#include <QtTest/QtTest>

#include "core/CalibrationController.h"
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

namespace {

constexpr quint64 k20mHz = 14200000ULL;

quint32 wireHz(const QByteArray& bank)
{
    return (quint32(quint8(bank[1])) << 24) | (quint32(quint8(bank[2])) << 16)
        | (quint32(quint8(bank[3])) << 8) | quint32(quint8(bank[4]));
}

QByteArray bank(const P1RadioConnection& conn, int index)
{
    quint8 out[5] = {};
    conn.composeCcForBankForTest(index, out);
    return QByteArray(reinterpret_cast<const char*>(out), 5);
}

} // namespace

class TestP1FrequencyCorrection : public QObject {
    Q_OBJECT
private slots:
    // Thetis applies Setup > Calibration's correction factor to every VFO
    // it sends, on Protocol 1 as Hz:
    //   From Thetis HPSDR/NetworkIO.cs:215-224 [v2.10.3.15] VFOfreq
    //     f_freq = (int)((f * 1e6) * _freq_correction_factor);
    //     if (CurrentRadioProtocol == RadioProtocol.USB)
    //         SetVFOfreq(id, f_freq, tx);  // sending freq Hz to firmware
    void receiveAndTransmitFrequenciesCarryTheCorrection()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setReceiverFrequency(0, k20mHz);
        conn.setTxFrequency(k20mHz);
        QCOMPARE(wireHz(bank(conn, 1)), quint32(k20mHz));
        QCOMPARE(wireHz(bank(conn, 2)), quint32(k20mHz));

        CalibrationController cal;
        cal.setFreqCorrectionFactor(1.00001);
        conn.setCalibrationController(&cal);
        // (int)(14200000 * 1.00001) = 14200142
        QCOMPARE(wireHz(bank(conn, 1)), 14200142u);
        QCOMPARE(wireHz(bank(conn, 2)), 14200142u);

        // The external 10 MHz reference uses its own factor.
        cal.setFreqCorrectionFactor10M(0.99999);
        cal.setUsing10MHzRef(true);
        QCOMPARE(wireHz(bank(conn, 2)), 14199858u);

        // A factor change reaches the next frame, as Thetis's
        // FreqCorrectionChanged re-sends the VFOs.
        cal.setUsing10MHzRef(false);
        cal.setFreqCorrectionFactor(1.0);
        QCOMPARE(wireHz(bank(conn, 2)), quint32(k20mHz));
    }
};

QTEST_MAIN(TestP1FrequencyCorrection)
#include "tst_p1_frequency_correction.moc"
