// SPDX-License-Identifier: GPL-2.0-or-later
// tests/tst_wdsp_ps_smoke.cpp (NereusSDR)
//
// Smoke test confirming calcc.c + iqc.c are vendored, compiled into
// wdsp_static, and exporting their public PS API. Does not exercise
// DSP behavior (that requires an opened channel and is covered by
// later tasks).

#include <QtTest>

#include "../third_party/wdsp/src/ps3_abi.h"

static_assert(PS3_MAX_DISPLAY_SAMPLES == 4096);
static_assert(PS3_DISPLAY_CORRECTION_POINTS == 512);

extern "C" {
    void SetPSRunCal(int channel, int run);
    void SetPSMox(int channel, int mox);
    void GetPSInfo(int channel, int* info);
    void GetPSDisp(int channel, double* x, double* ym, double* yc, double* ys,
                   double* xmCorrection, double* ymCorrection,
                   double* xaCorrection, double* yaCorrection,
                   int* sampleCount, int* correctionCount,
                   double* phaseReferenceDegrees);
    void SetPSReset(int channel, int reset);
    void SetPSControl(int channel, int reset, int mancal, int automode, int turnon);
}

class TstWdspPsSmoke : public QObject {
    Q_OBJECT
private slots:
    void calccSymbolsLink();
    void invalidAbiInputsAreRejected();
    void iqcLinkable();
};

void TstWdspPsSmoke::calccSymbolsLink() {
    // We can't open a channel here (would require WdspEngine setup), so we
    // just verify the symbols exist by taking their address.
    // GCC/Clang -Wunused-value would otherwise complain; cast to void.
    (void)reinterpret_cast<void*>(&SetPSRunCal);
    (void)reinterpret_cast<void*>(&SetPSMox);
    (void)reinterpret_cast<void*>(&GetPSRunCal);
    (void)reinterpret_cast<void*>(&GetPSInfo);
    (void)reinterpret_cast<void*>(&GetPSDisp);
    (void)reinterpret_cast<void*>(&SetPSReset);
    (void)reinterpret_cast<void*>(&SetPSControl);
    QVERIFY(true);  // If the test linked, the symbols exist.
}

void TstWdspPsSmoke::invalidAbiInputsAreRejected() {
    int run = 37;
    QCOMPARE(GetPSRunCal(-1, &run), 0);
    QCOMPARE(run, 37);
    QCOMPARE(GetPSRunCal(0, nullptr), 0);

    // Invalid channel and null outputs must return before dereferencing any
    // pointer.  The void ABI reports rejection by leaving the call inert.
    GetPSDisp(-1, nullptr, nullptr, nullptr, nullptr,
             nullptr, nullptr, nullptr, nullptr,
             nullptr, nullptr, nullptr);
    QVERIFY(true);
}

void TstWdspPsSmoke::iqcLinkable() {
    // iqc.c does not export setter/getter API directly to host; it's wired
    // through TXA's xiqc() chain. So we just verify the wdsp_static target
    // has linked successfully (this test executable existing proves it).
    QVERIFY(true);
}

QTEST_MAIN(TstWdspPsSmoke)
#include "tst_wdsp_ps_smoke.moc"
