// =================================================================
// tests/tst_rxchannel_notch_wrappers.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis and WDSP
// file names appear in comments to document what each wrapper forwards to;
// no upstream logic is ported into this file.
//
// Tunable Notch Filter, Task 2: the Notch value type plus the RxChannel
// manual-notch wrappers that carry it into the per-channel WDSP notch
// database.
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.1 (Notch), 6.1 (wdsp_api.h declarations),
//         6.2 (RxChannel wrappers), 11.1 (why these need a real channel).
// =================================================================
#include <QtTest/QtTest>

#include "core/dsp/Notch.h"

using namespace NereusSDR;

class TestRxChannelNotchWrappers : public QObject {
    Q_OBJECT

private slots:
    // -- 5.1: the Notch value type ----------------------------------------

    void notch_defaults_to_panadapter_width()
    {
        Notch n;
        QCOMPARE(n.widthHz, 200.0);
    }

    void notch_defaults_to_active()
    {
        Notch n;
        QVERIFY(n.active);
    }

    void notch_defaults_to_unset_id_and_centre()
    {
        Notch n;
        QCOMPARE(n.id, 0);
        QCOMPARE(n.centerHz, 0.0);
    }
};

QTEST_MAIN(TestRxChannelNotchWrappers)
#include "tst_rxchannel_notch_wrappers.moc"
