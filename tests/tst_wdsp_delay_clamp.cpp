// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - the PureSignal amp-delay line cannot read outside its ring.
//
// Ported from Longpath tests/tst_wdsp_delay_clamp.cpp [@383f058]
// (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath, PR 25,
// commit 7b4450a1; no top-of-file GPL header upstream, the project-level
// LICENSE applies). The four cases and their comments are Longpath's,
// kept verbatim. They exercise the bound in
// third_party/wdsp/src/delay.c (see its modification history).
//
// Modification history (NereusSDR):
//   2026-10-22: Ported from Longpath by J.J. Boyd (KG4VCF), with
//               AI-assisted transformation via Claude Code.
//
// --- From tst_wdsp_delay_clamp.cpp (Longpath) ---
//
// tests/tst_wdsp_delay_clamp.cpp (NereusSDR)
//
// The PureSignal amp-delay line (WDSP delay.c) can hold WSDEL - 1 whole
// samples, and the operator's field allows 25 ms — at 192 kHz that is
// nearly five times what the ring holds. Before 2026-09-20 the requested
// delay was turned into a ring offset without a bound, and xdelay() wraps
// its read index once, so anything past the ring read heap memory beyond
// the allocation. The bound in third_party/wdsp/src/delay.c (see its
// modification history) keeps the offset inside the ring and reports the
// delay that was actually realised.
//
// These tests go through the public delay API only, with the same
// geometry calcc.c uses (tdelta = 20 ns), and read nothing from the
// struct: what they check is what the caller can observe — the value
// SetDelayValue() returns and where an impulse comes out.
// [upstream line 17, a marker exempting the file from the port checks as
//  NereusSDR-original, is left out: in NereusSDR this file is a
//  registered Longpath port]

#include <QtTest/QtTest>

#include <cmath>
#include <vector>

extern "C" {
struct _delay;
typedef struct _delay* DELAY;
DELAY  create_delay(int run, int size, double* in, double* out, int rate,
                    double tdelta, double tdelay);
void   destroy_delay(DELAY a);
void   xdelay(DELAY a);
double SetDelayValue(DELAY a, double tdelay);
}

namespace {

// delay.h: number of supported whole-sample delays.
constexpr int    kWsdel  = 1025;
constexpr int    kRate   = 192000;      // TX DSP rate of a Protocol-2 radio
constexpr double kTdelta = 20.0e-9;     // calcc.c:196 — 20 ns step size
constexpr int    kSize   = 64;

// The same arithmetic as create_delay(), so an expectation can be stated
// in the caller's units without reaching into the struct.
struct Geometry {
    int    L;
    double adelta;
    int    cpp;
    Geometry()
    {
        L = static_cast<int>(0.5 + 1.0 / (kTdelta * double(kRate)));
        adelta = 1.0 / (double(kRate) * L);
        const double ft = 0.45 / double(L);
        int ncoef = static_cast<int>(60.0 / ft);
        ncoef = (ncoef / L + 1) * L;
        cpp = ncoef / L;
    }
    double maxDelay() const
    {
        return adelta * double((kWsdel - 1) * L + (L - 1));
    }
};

// Push an impulse through the line and return the index (in samples from
// the impulse) at which the output peaks, plus the peak itself.
struct Impulse { int at; double peak; bool finite; };

Impulse impulseThrough(DELAY a, double* in, double* out, int samples)
{
    Impulse r{-1, 0.0, true};
    int n = 0;
    for (int block = 0; n < samples; ++block) {
        for (int i = 0; i < kSize; ++i) {
            in[2 * i]     = (block == 0 && i == 0) ? 1.0 : 0.0;
            in[2 * i + 1] = 0.0;
        }
        xdelay(a);
        for (int i = 0; i < kSize; ++i, ++n) {
            const double v = std::fabs(out[2 * i]);
            if (!std::isfinite(out[2 * i]) || !std::isfinite(out[2 * i + 1])) {
                r.finite = false;
            }
            if (v > r.peak) { r.peak = v; r.at = n; }
        }
    }
    return r;
}

} // namespace

class TstWdspDelayClamp : public QObject {
    Q_OBJECT
private slots:
    void a_delay_the_ring_can_hold_is_realised_as_requested();
    void a_delay_beyond_the_ring_is_clamped_and_the_clamp_is_reported();
    void the_clamped_line_delays_an_impulse_by_exactly_the_ring();
    void a_negative_delay_becomes_zero();
};

void TstWdspDelayClamp::a_delay_the_ring_can_hold_is_realised_as_requested()
{
    // The regression guard for the port itself: a delay well inside the
    // ring must come back as before, to within one step.
    const Geometry g;
    std::vector<double> in(2 * kSize), out(2 * kSize);
    DELAY a = create_delay(1, kSize, in.data(), out.data(), kRate, kTdelta, 0.0);

    const double requested = 1.0e-3;    // 192 samples
    const double realised  = SetDelayValue(a, requested);
    QVERIFY2(std::fabs(realised - requested) <= g.adelta,
             qPrintable(QStringLiteral("realised %1 for %2")
                            .arg(realised).arg(requested)));

    // And the impulse comes out where the delay says, plus the
    // interpolation filter's own centre (cpp / 2 samples).
    const Impulse r = impulseThrough(a, in.data(), out.data(), 4 * kSize + 4096);
    QVERIFY(r.finite);
    const int snum = 192;
    QVERIFY2(r.at >= snum && r.at <= snum + g.cpp,
             qPrintable(QStringLiteral("impulse peaked at %1, expected %2..%3")
                            .arg(r.at).arg(snum).arg(snum + g.cpp)));
    QVERIFY(r.peak > 0.5);
    destroy_delay(a);
}

void TstWdspDelayClamp::a_delay_beyond_the_ring_is_clamped_and_the_clamp_is_reported()
{
    const Geometry g;
    std::vector<double> in(2 * kSize), out(2 * kSize);
    DELAY a = create_delay(1, kSize, in.data(), out.data(), kRate, kTdelta, 0.0);

    // 25 ms is the top of the PureSignal amp-delay field (PsForm.cpp).
    const double realised = SetDelayValue(a, 25.0e-3);
    QVERIFY2(realised < 25.0e-3, "a delay the ring cannot hold was reported as realised");
    QVERIFY2(std::fabs(realised - g.maxDelay()) < 1e-12,
             qPrintable(QStringLiteral("clamped to %1, the ring holds %2")
                            .arg(realised).arg(g.maxDelay())));
    // 1024 whole samples at 192 kHz: 5.33 ms.
    QVERIFY(realised > 5.3e-3 && realised < 5.4e-3);
    destroy_delay(a);
}

void TstWdspDelayClamp::the_clamped_line_delays_an_impulse_by_exactly_the_ring()
{
    // What the operator hears with the clamp in place: the longest delay
    // the line can do, not silence and not memory from beyond the ring.
    // Before the clamp this walked xdelay() off the end of the
    // allocation (AddressSanitizer catches it; without ASAN it is heap
    // garbage or a crash).
    const Geometry g;
    std::vector<double> in(2 * kSize), out(2 * kSize);
    DELAY a = create_delay(1, kSize, in.data(), out.data(), kRate, kTdelta, 0.0);
    SetDelayValue(a, 25.0e-3);

    const Impulse r = impulseThrough(a, in.data(), out.data(), 4 * kSize + 4096);
    QVERIFY(r.finite);
    const int snum = kWsdel - 1;
    QVERIFY2(r.at >= snum && r.at <= snum + g.cpp,
             qPrintable(QStringLiteral("impulse peaked at %1, expected %2..%3")
                            .arg(r.at).arg(snum).arg(snum + g.cpp)));
    QVERIFY2(r.peak > 0.5,
             qPrintable(QStringLiteral("peak %1").arg(r.peak)));
    destroy_delay(a);
}

void TstWdspDelayClamp::a_negative_delay_becomes_zero()
{
    // calcc.c never hands a negative value down (it swaps to the RX line
    // instead), but the line itself must not turn one into a negative
    // ring offset either.
    std::vector<double> in(2 * kSize), out(2 * kSize);
    DELAY a = create_delay(1, kSize, in.data(), out.data(), kRate, kTdelta, 0.0);
    QCOMPARE(SetDelayValue(a, -1.0e-3), 0.0);
    const Impulse r = impulseThrough(a, in.data(), out.data(), 4 * kSize);
    QVERIFY(r.finite);
    QVERIFY(r.peak > 0.5);
    destroy_delay(a);
}

QTEST_APPLESS_MAIN(TstWdspDelayClamp)
#include "tst_wdsp_delay_clamp.moc"
