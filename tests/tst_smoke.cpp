// tst_smoke.cpp — proves NereusSDR test infrastructure links and runs.
// Replace once real tests exist; this exists so commit 1 (test scaffolding)
// has something to verify the build wiring.

#include <QtTest/QtTest>

class TstSmoke : public QObject
{
    Q_OBJECT

private slots:
    void infraIsAlive()
    {
        QCOMPARE(1 + 1, 2);
    }
};

QTEST_MAIN(TstSmoke)
#include "tst_smoke.moc"
