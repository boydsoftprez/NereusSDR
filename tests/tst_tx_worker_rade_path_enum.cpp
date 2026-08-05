// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_tx_worker_rade_path_enum.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxWorkerThread::TxPath enum + currentTxPath
// scaffolding (Phase 3R Task K2).
//
// K2 adds the lightweight enum + std::atomic member + cross-thread
// setter slot that future K-bench work will use to route a slice
// in DSPMode::RADE_U / RADE_L through RadeChannel rather than the WDSP
// TXA chain.  This commit pins only the contract:
//
//   * TxPath has exactly two values, Wdsp and Rade.
//   * m_currentTxPath defaults to Wdsp (so existing WDSP-only TX
//     paths stay unaffected for non-RADE modes).
//   * setCurrentTxPath atomically swaps the member.
//
// The full RADE-path integration (mic feed -> HPF -> 48-16
// resampler -> RadeChannel::txEncode) is deferred to a K-bench
// follow-up; this test deliberately does not exercise the
// run-loop branch.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-11 — Phase 3R Task K2: initial test file. NereusSDR-native.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/TxWorkerThread.h"

using namespace NereusSDR;

class TstTxWorkerRadePathEnum : public QObject {
    Q_OBJECT

private slots:

    // Default path is Wdsp so non-RADE modes (USB, LSB, AM, CW, etc.)
    // keep flowing through the existing TXA chain unchanged.
    void defaultsToWdsp()
    {
        TxWorkerThread worker;
        QCOMPARE(worker.currentTxPathForTest(), TxWorkerThread::TxPath::Wdsp);
    }

    // setCurrentTxPath flips the atomic member.  Run the swap a few
    // times to make sure both directions land.
    void setCurrentTxPathSwitchesAtomically()
    {
        TxWorkerThread worker;

        worker.setCurrentTxPath(TxWorkerThread::TxPath::Rade);
        QCOMPARE(worker.currentTxPathForTest(), TxWorkerThread::TxPath::Rade);

        worker.setCurrentTxPath(TxWorkerThread::TxPath::Wdsp);
        QCOMPARE(worker.currentTxPathForTest(), TxWorkerThread::TxPath::Wdsp);

        worker.setCurrentTxPath(TxWorkerThread::TxPath::Rade);
        QCOMPARE(worker.currentTxPathForTest(), TxWorkerThread::TxPath::Rade);
    }
};

QTEST_MAIN(TstTxWorkerRadePathEnum)
#include "tst_tx_worker_rade_path_enum.moc"
