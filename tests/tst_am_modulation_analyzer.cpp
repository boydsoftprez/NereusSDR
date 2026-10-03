// no-port-check: test-only — exercises NereusSDR-native AmModulationAnalyzer.
// Feeds synthetic AM I/Q (carrier + audio) and checks the peak-reading
// modulation percentages, carrier tracking, reset, and the scope trace.
#include <QtTest>
#include "core/AmModulationAnalyzer.h"
#include "gui/applets/ModMonitorApplet.h"

#include <cmath>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFs = 48000;

// AM in WDSP ammod form: out = c + a * audio, on both I and Q (mode 0).
// With c = 0.5 and audio in [-1, 1] the envelope is 0.5 + 0.5*audio.
std::vector<float> makeAm(double seconds, double audioHz,
                          double posAmp, double negAmp, double c = 0.5)
{
    const int n = static_cast<int>(seconds * kFs);
    std::vector<float> iq(static_cast<std::size_t>(n) * 2);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kFs;
        double audio = std::sin(2.0 * M_PI * audioHz * t);
        audio *= (audio >= 0.0) ? posAmp : negAmp;   // asymmetric audio
        const double env = c + (1.0 - c) * audio;
        // Put the envelope on I with a small quadrature so sqrt(I^2+Q^2)
        // is exercised, not just |I|.
        iq[2 * i + 0] = static_cast<float>(env * 0.8);
        iq[2 * i + 1] = static_cast<float>(env * 0.6);
    }
    return iq;
}

void feed(AmModulationAnalyzer& a, const std::vector<float>& iq, int block = 64)
{
    const int frames = static_cast<int>(iq.size() / 2);
    for (int off = 0; off + block <= frames; off += block) {
        a.pushIq(iq.data() + 2 * off, block);
    }
}

} // namespace

class TestAmModulationAnalyzer : public QObject
{
    Q_OBJECT
private slots:
    void symmetricTone_readsEqualPeaks()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        feed(a, makeAm(1.0, 1000.0, 0.8, 0.8));
        const auto s = a.snapshot();
        QVERIFY(s.carrierPresent);
        QVERIFY(!s.carrierLow);
        QVERIFY(!s.carrierHigh);
        // Carrier 0.5 * |(0.8, 0.6)| = 0.5
        QVERIFY2(std::fabs(s.carrierLevel - 0.5) < 0.01, qPrintable(QString::number(s.carrierLevel)));
        QVERIFY2(std::fabs(s.posPeakPct - 80.0) < 2.0, qPrintable(QString::number(s.posPeakPct)));
        QVERIFY2(std::fabs(s.negPeakPct - 80.0) < 2.0, qPrintable(QString::number(s.negPeakPct)));
        QVERIFY(s.posHoldPct >= 78.0);
        QVERIFY(s.negHoldPct >= 78.0);
        QVERIFY(!s.scope.empty());
        QCOMPARE(s.scopeRateHz, kFs / 6);
    }

    void asymmetricAudio_readsPositiveOverNegative()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        // +125 % positive, 60 % negative: the classic "AM with asymmetry" case.
        feed(a, makeAm(1.0, 400.0, 1.25, 0.60));
        const auto s = a.snapshot();
        QVERIFY(s.carrierPresent);
        // Carrier tracker sees the asymmetric mean (0.5 + 0.5*mean(audio));
        // mean(audio) = (1.25 - 0.60) * (2/pi) / 2 ≈ 0.207 → carrier ≈ 0.603.
        // Peaks relative to that carrier: +(1.125/0.603-1) ≈ 86.5 %,
        // -(1 - 0.20/0.603) ≈ 66.8 %.  A hardware detector reads the same
        // thing, since it also uses the DC level as the carrier reference.
        QVERIFY2(s.posPeakPct > s.negPeakPct + 10.0,
                 qPrintable(QStringLiteral("pos=%1 neg=%2").arg(s.posPeakPct).arg(s.negPeakPct)));
        QVERIFY(s.posPeakPct > 80.0 && s.posPeakPct < 95.0);
        QVERIFY(s.negPeakPct > 60.0 && s.negPeakPct < 75.0);
    }

    void unmodulatedCarrier_readsZero()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        feed(a, makeAm(0.5, 1000.0, 0.0, 0.0));
        const auto s = a.snapshot();
        QVERIFY(s.carrierPresent);
        QVERIFY(s.posPeakPct < 0.5);
        QVERIFY(s.negPeakPct < 0.5);
    }

    void silence_reportsNoCarrier()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        std::vector<float> zeros(2 * 4800, 0.0f);
        feed(a, zeros);
        const auto s = a.snapshot();
        QVERIFY(!s.carrierPresent);
        QCOMPARE(s.posPeakPct, 0.0);
        QCOMPARE(s.negPeakPct, 0.0);
    }

    void snapshot_consumesWindowPeaksButKeepsHold()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        a.setPeakHoldMs(60000.0);
        feed(a, makeAm(0.5, 1000.0, 0.8, 0.8));
        const auto s1 = a.snapshot();
        QVERIFY(s1.posPeakPct > 70.0);
        // Now feed an unmodulated carrier: window peak drops, hold stays.
        feed(a, makeAm(0.2, 1000.0, 0.0, 0.0));
        const auto s2 = a.snapshot();
        QVERIFY(s2.posPeakPct < 5.0);
        QVERIFY(s2.posHoldPct > 70.0);
    }

    void reset_clearsEverything()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        feed(a, makeAm(0.5, 1000.0, 0.8, 0.8));
        a.reset();
        const auto s = a.snapshot();
        QVERIFY(!s.carrierPresent);
        QCOMPARE(s.posHoldPct, 0.0);
        QCOMPARE(s.negHoldPct, 0.0);
        QVERIFY(s.scope.empty());
        QCOMPARE(s.framesSeen, static_cast<std::uint64_t>(0));
    }

    void applet_flashersAndLampFollowSnapshot()
    {
        ModMonitorApplet applet(nullptr);
        AmModulationAnalyzer::Snapshot s;
        s.carrierPresent = true;
        s.carrierLevel = 0.5;
        s.posPeakPct = 130.0;   // above default 125 % flasher
        s.negPeakPct = 50.0;
        s.posHoldPct = 130.0;
        s.negHoldPct = 50.0;
        applet.applySnapshotForTest(s);
        QVERIFY(applet.posFlasherLitForTest());
        QVERIFY(!applet.negFlasherLitForTest());
        QCOMPARE(applet.carrierLampTextForTest(), QStringLiteral("CARRIER OK"));

        AmModulationAnalyzer::Snapshot off;
        applet.applySnapshotForTest(off);
        QCOMPARE(applet.carrierLampTextForTest(), QStringLiteral("NO CARRIER"));
        // Flasher latches until reset.
        QVERIFY(applet.posFlasherLitForTest());
        applet.resetPeaks();
        QVERIFY(!applet.posFlasherLitForTest());
    }
};

QTEST_MAIN(TestAmModulationAnalyzer)
#include "tst_am_modulation_analyzer.moc"
