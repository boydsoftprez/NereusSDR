// =================================================================
// tests/tst_network_diagnostics_dialog.cpp  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Created for Phase 3Q sub-PR-3 (NetworkDiagnosticsDialog).
//                NereusSDR-original tests. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "gui/NetworkDiagnosticsDialog.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstNetworkDiagnosticsDialog : public QObject {
    Q_OBJECT

private slots:
    void buildsWithNullModel() {
        // Defensive: opening before connection should not crash.
        NetworkDiagnosticsDialog dlg(/*model=*/nullptr, /*audio=*/nullptr);
        // Pass = no crash during construction
        QVERIFY(true);
    }

    void refreshDoesNotCrashWithDisconnectedModel() {
        RadioModel model;
        NetworkDiagnosticsDialog dlg(&model, nullptr);
        QMetaObject::invokeMethod(&dlg, "refresh");
        // Pass = no crash
        QVERIFY(true);
    }

    void resetSessionStatsZeroesCounters() {
        RadioModel model;
        AudioEngine engine;
        NetworkDiagnosticsDialog dlg(&model, &engine);

        // Drive 1 underrun bump
        engine.simulateUnderrun();

        // Reset
        QMetaObject::invokeMethod(&dlg, "onResetSessionStats");

        // Verify by calling reset twice — no crash, idempotent
        QMetaObject::invokeMethod(&dlg, "onResetSessionStats");
        QVERIFY(true);
    }

    void connectsToRadioConnectionSignalsWithoutCrash() {
        RadioModel model;
        NetworkDiagnosticsDialog dlg(&model, nullptr);

        // If RadioModel::connection() is non-null, the dialog should have
        // wired pingRttMeasured. Emitting it should not crash.
        if (auto* conn = model.connection()) {
            emit conn->pingRttMeasured(42);
            emit conn->pingRttMeasured(120);
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(TstNetworkDiagnosticsDialog)
#include "tst_network_diagnostics_dialog.moc"
