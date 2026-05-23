// =================================================================
// tests/tst_tuner_model_apply_status.cpp  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Test scaffolding for TunerModel::applyStatus by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Based on AetherSDR src/models/TunerModel.{h,cpp}
//                 [@0cd4559].
// =================================================================

#include <QtTest/QtTest>
#include "models/TunerModel.h"

class TunerModelApplyStatusTest : public QObject {
    Q_OBJECT
private slots:
    void appliesRelayValues();
    void appliesOperateAndBypass();
    void appliesAntennaSwitchModel();
    void appliesMeters();
    void emitsPresenceOnFirstStatus();
};

void TunerModelApplyStatusTest::appliesRelayValues() {
    NereusSDR::TunerModel m;
    QSignalSpy relaySpy(&m, &NereusSDR::TunerModel::relayChanged);

    m.applyStatus({{"relayC1","42"},{"relayL","199"},{"relayC2","88"}});

    QCOMPARE(m.relayC1(), 42);
    QCOMPARE(m.relayL(),  199);
    QCOMPARE(m.relayC2(), 88);
    QVERIFY(relaySpy.count() >= 1);
}

void TunerModelApplyStatusTest::appliesOperateAndBypass() {
    NereusSDR::TunerModel m;
    m.applyStatus({{"operate","1"},{"bypass","0"}});
    QVERIFY(m.isOperate());
    QVERIFY(!m.isBypass());

    m.applyStatus({{"bypass","1"}});
    QVERIFY(m.isBypass());
}

void TunerModelApplyStatusTest::appliesAntennaSwitchModel() {
    NereusSDR::TunerModel m;
    QVERIFY(!m.hasAntennaSwitch());
    m.applyStatus({{"one_by_three","1"},{"antA","1"}});
    QVERIFY(m.hasAntennaSwitch());
    QCOMPARE(m.antennaA(), 1);
}

void TunerModelApplyStatusTest::appliesMeters() {
    NereusSDR::TunerModel m;
    QSignalSpy metersSpy(&m, &NereusSDR::TunerModel::metersChanged);
    m.applyStatus({{"fwd","12.5"},{"swr","1.4"}});
    QCOMPARE(m.fwdPower(), 12.5f);
    QCOMPARE(m.swr(),      1.4f);
    QCOMPARE(metersSpy.count(), 1);
}

void TunerModelApplyStatusTest::emitsPresenceOnFirstStatus() {
    NereusSDR::TunerModel m;
    QSignalSpy presenceSpy(&m, &NereusSDR::TunerModel::presenceChanged);
    m.applyStatus({{"model","TunerGeniusXL"},{"serial_num","TGXL1234"}});
    QVERIFY(m.isPresent());
    QCOMPARE(presenceSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TunerModelApplyStatusTest)
#include "tst_tuner_model_apply_status.moc"
