// no-port-check: NereusSDR-original characterization of the TX EQ's public
// profile API; no upstream algorithm is duplicated in this test.
// =================================================================
// tests/tst_wdsp_txa_eq_q.cpp  (NereusSDR)
// =================================================================
//
// R-R3-49 (group A fix wave): SetTXAEQProfile takes Q as Thetis's does
// (dsp.cs:788, wdsp/eq.c:780 [v2.10.3.15]). A profile without Q (the legacy
// EQ, and the parametric panel with Q factors off) must build the same
// impulse as the WDSP 2.10 eq.c before the change (WdspEqReference.h,
// captured from that binary); a profile with Q must build a finite,
// non-flat impulse, different from the same points without Q, for 5, 10
// and 18 points, and keep its Q through a rebuild. A real TX channel, no
// radio: nothing is keyed.
//
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Group B fix wave: a 2 or 3 point Q
//                                    profile rebuilds on a cut-off mode or
//                                    window change. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================
#include <QtTest>
#include <QTemporaryDir>
#include "WdspEqReference.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/wdsp_api.h"

extern "C" int nereus_copy_txa_eq_impulse(int channel, double* impulse, int capacity);
extern "C" double nereus_txa_eq_samplerate(int channel);

using namespace NereusSDR;

namespace {

constexpr int kChannel = 1;

struct Profile {
    std::vector<double> F;
    std::vector<double> G;
};

// The three profiles WdspEqReference.h was captured with, in its order.
const Profile kReferenceProfiles[] = {
    {{0, 32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000},
     {0, -12, -12, -12, -1, 1, 4, 9, 12, -10, -10}},
    {{0, 50, 100, 200, 400, 700, 1100, 1600, 2200, 2600, 3000},
     {3, -6, -3, 0, 2, 4, 6, 5, 3, -2, -8}},
    {{0, 0, 675, 1350, 2025, 2700},
     {1.5, -4, 6.5, -4, 6.5, -4}},
};

std::vector<double> impulse()
{
    std::vector<double> out(1 << 16);
    const int n = nereus_copy_txa_eq_impulse(kChannel, out.data(), static_cast<int>(out.size()));
    out.resize(static_cast<std::size_t>(std::max(n, 0)));
    return out;
}

// |H(f)| in dB of the real impulse h[n] = coef[2n].
double responseDb(const std::vector<double>& coef, double samplerate, double hz)
{
    std::complex<double> sum(0.0, 0.0);
    const std::size_t n = coef.size() / 2;
    for (std::size_t i = 0; i < n; ++i) {
        const double phase = -2.0 * M_PI * hz * static_cast<double>(i) / samplerate;
        sum += coef[2 * i] * std::complex<double>(std::cos(phase), std::sin(phase));
    }
    return 20.0 * std::log10(std::max(std::abs(sum), 1e-30));
}

// A parametric panel's points as Thetis's sendTXDspUpdate hands them to
// WDSP: F[0] = 0, G[0] = the preamp, Q[0] = 0, then every point.
void parametric(int points, std::vector<double>& F, std::vector<double>& G,
                std::vector<double>& Q)
{
    F.assign(1, 0.0);
    G.assign(1, 1.5);
    Q.assign(1, 0.0);
    for (int i = 0; i < points; ++i) {
        F.push_back(2700.0 * i / (points - 1));
        G.push_back((i % 2) ? 9.0 : -6.0);
        Q.push_back(2.0 + 0.5 * (i % 3));
    }
}

}  // namespace

class TestWdspTxaEqQ : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        m_engine = std::make_unique<WdspEngine>();
        m_engine->setSynchronousInitForTest(true);
        QVERIFY(m_engine->initialize(m_directory.path() + QLatin1Char('/')));
        m_tx = m_engine->createTxChannel(kChannel);
        QVERIFY(m_tx);
        QVERIFY(nereus_txa_eq_samplerate(kChannel) > 0.0);
    }

    // No Q: the impulse the WDSP 2.10 eq.c built before the Q path existed.
    void profilesWithoutQAreUnchanged()
    {
        for (int p = 0; p < 3; ++p) {
            Profile profile = kReferenceProfiles[p];
            SetTXAEQProfile(kChannel, static_cast<int>(profile.F.size()) - 1,
                            profile.F.data(), profile.G.data(), nullptr);
            verifyReference(p);
        }
    }

    void qProfileBuildsANonFlatImpulse_data()
    {
        QTest::addColumn<int>("points");
        QTest::newRow("5 points") << 5;
        QTest::newRow("10 points") << 10;
        QTest::newRow("18 points") << 18;
    }

    void qProfileBuildsANonFlatImpulse()
    {
        QFETCH(int, points);
        const double fs = nereus_txa_eq_samplerate(kChannel);
        std::vector<double> F;
        std::vector<double> G;
        std::vector<double> Q;
        parametric(points, F, G, Q);

        SetTXAEQProfile(kChannel, points, F.data(), G.data(), nullptr);
        const std::vector<double> withoutQ = impulse();

        SetTXAEQProfile(kChannel, points, F.data(), G.data(), Q.data());
        const std::vector<double> withQ = impulse();
        QCOMPARE(withQ.size(), withoutQ.size());
        QVERIFY(!withQ.empty());
        double peak = 0.0;
        double diff = 0.0;
        for (std::size_t i = 0; i < withQ.size(); ++i) {
            QVERIFY(std::isfinite(withQ[i]));
            peak = std::max(peak, std::abs(withQ[i]));
            diff = std::max(diff, std::abs(withQ[i] - withoutQ[i]));
        }
        QVERIFY(peak > 0.0);
        // The Q branch, not the spline between the points.
        QVERIFY2(diff > 1e-3 * peak, "the Q profile built the same impulse as without Q");

        // Non-flat: a peak point and a dip point of the curve differ.
        double lo = 1e9;
        double hi = -1e9;
        for (int i = 1; i <= points; ++i) {
            if (F[i] <= 0.0) { continue; }
            const double db = responseDb(withQ, fs, F[i]);
            QVERIFY(std::isfinite(db));
            lo = std::min(lo, db);
            hi = std::max(hi, db);
        }
        QVERIFY2(hi - lo > 3.0, qPrintable(QStringLiteral("spread %1 dB").arg(hi - lo)));

        // A rebuild (the cut-off mode) keeps the Q.
        SetTXAEQCtfmode(kChannel, 0);
        QCOMPARE(impulse(), withQ);
    }

    // Group A follow-up (group B fix wave): a Q profile of 2 or 3 points
    // is rebuilt on a cut-off mode or window change too. Thetis's
    // SetTXAEQCtfmode and SetTXAEQWintype always rebuild (wdsp/eq.c:808-829
    // [v2.10.3.15]); the vendored ones rebuilt only when the spline check
    // passed, which needs 4 points or more.
    void shortQProfileRebuildsOnCtfmodeAndWindow_data()
    {
        QTest::addColumn<int>("points");
        QTest::newRow("2 points") << 2;
        QTest::newRow("3 points") << 3;
    }

    void shortQProfileRebuildsOnCtfmodeAndWindow()
    {
        QFETCH(int, points);
        std::vector<double> F;
        std::vector<double> G;
        std::vector<double> Q;
        parametric(points, F, G, Q);
        // The TX EQ's own cut-off mode and window (TXA.c create_eqp: 0 and
        // 2), put back whatever happens, for the reference cases.
        const auto restore = qScopeGuard([] {
            SetTXAEQCtfmode(kChannel, 0);
            SetTXAEQWintype(kChannel, 2);
        });
        SetTXAEQCtfmode(kChannel, 0);
        SetTXAEQWintype(kChannel, 2);
        SetTXAEQProfile(kChannel, points, F.data(), G.data(), Q.data());
        const std::vector<double> base = impulse();

        SetTXAEQCtfmode(kChannel, 1);
        const std::vector<double> otherCutoff = impulse();
        QVERIFY2(otherCutoff != base, "the cut-off mode change left the impulse as it was");
        SetTXAEQCtfmode(kChannel, 0);
        QCOMPARE(impulse(), base);

        SetTXAEQWintype(kChannel, 1);
        const std::vector<double> otherWindow = impulse();
        QVERIFY2(otherWindow != base, "the window change left the impulse as it was");
        SetTXAEQWintype(kChannel, 2);
        QCOMPARE(impulse(), base);
    }

    // A profile without Q after one with it clears the Q: the legacy EQ
    // after the parametric panel builds its old impulse again.
    void legacyAfterQIsUnchanged()
    {
        std::vector<double> F;
        std::vector<double> G;
        std::vector<double> Q;
        parametric(18, F, G, Q);
        SetTXAEQProfile(kChannel, 18, F.data(), G.data(), Q.data());
        Profile legacy = kReferenceProfiles[0];
        SetTXAEQProfile(kChannel, 10, legacy.F.data(), legacy.G.data(), nullptr);
        verifyReference(0);

        // And the ten-band graphic EQ clears it too, as Thetis's does.
        SetTXAEQProfile(kChannel, 18, F.data(), G.data(), Q.data());
        int txeq[11] = {0, -12, -12, -12, -1, 1, 4, 9, 12, -10, -10};
        SetTXAGrphEQ10(kChannel, txeq);
        verifyReference(0);
    }

    void cleanupTestCase()
    {
        m_tx = nullptr;
        m_engine.reset();
    }

private:
    void verifyReference(int profile)
    {
        const std::vector<double> coef = impulse();
        for (const auto& e : kEqEnergyReference) {
            if (e.profile != profile) { continue; }
            QCOMPARE(static_cast<int>(coef.size()), e.count);
            double energy = 0.0;
            for (double v : coef) { energy += v * v; }
            QVERIFY2(std::abs(energy - e.energy) <= 1e-9 * e.energy,
                     qPrintable(QStringLiteral("profile %1 energy %2 vs %3")
                                    .arg(profile).arg(energy, 0, 'g', 17)
                                    .arg(e.energy, 0, 'g', 17)));
        }
        double peak = 0.0;
        for (double v : coef) { peak = std::max(peak, std::abs(v)); }
        int compared = 0;
        for (const auto& r : kEqImpulseReference) {
            if (r.profile != profile) { continue; }
            const double actual = coef.at(static_cast<std::size_t>(r.index));
            QVERIFY2(std::abs(actual - r.value) <= 1e-9 * peak + 1e-9 * std::abs(r.value),
                     qPrintable(QStringLiteral("profile %1 index %2: %3 vs %4")
                                    .arg(profile).arg(r.index).arg(actual, 0, 'g', 17)
                                    .arg(r.value, 0, 'g', 17)));
            ++compared;
        }
        QCOMPARE(compared, 128);
    }

    QTemporaryDir m_directory;
    std::unique_ptr<WdspEngine> m_engine;
    TxChannel* m_tx = nullptr;
};

QTEST_MAIN(TestWdspTxaEqQ)
#include "tst_wdsp_txa_eq_q.moc"
