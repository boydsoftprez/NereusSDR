// tests/tst_smeter_widget_external_amp.cpp
//
// Pins the contract that SMeterWidget subscribes to the cross-vendor
// externalAmpOperateChanged signal on RadioModel rather than the
// PGXL-specific PgxlConnection::statusUpdated signal directly.
//
// Both PGXL and RF-Kit OPERATE transitions must flip the S-meter to
// the 2 kW scale; STANDBY from all amps must revert it to barefoot.
//
// Test seam: txScaleIs2kWForTesting() reads m_powerScaleMax >= 2000.0f.
// Task 13 -- Phase 3P-III.

#include <QtTest>
#include "gui/SMeterWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class SMeterWidgetExternalAmpTest : public QObject {
    Q_OBJECT
private slots:
    void scaleFlipsOnExternalAmpOperate();
    void scaleRevertsWhenAllAmpsStandby();
};

void SMeterWidgetExternalAmpTest::scaleFlipsOnExternalAmpOperate()
{
    RadioModel m;
    SMeterWidget w(&m);
    QVERIFY(!w.txScaleIs2kWForTesting());
    emit m.externalAmpOperateChanged(true);
    QVERIFY(w.txScaleIs2kWForTesting());
}

void SMeterWidgetExternalAmpTest::scaleRevertsWhenAllAmpsStandby()
{
    RadioModel m;
    SMeterWidget w(&m);
    emit m.externalAmpOperateChanged(true);
    QVERIFY(w.txScaleIs2kWForTesting());
    emit m.externalAmpOperateChanged(false);
    QVERIFY(!w.txScaleIs2kWForTesting());
}

QTEST_MAIN(SMeterWidgetExternalAmpTest)
#include "tst_smeter_widget_external_amp.moc"
