// tests/tst_station_block.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <QLabel>
#include "gui/widgets/StationBlock.h"

using namespace NereusSDR;

class TstStationBlock : public QObject {
    Q_OBJECT

private slots:
    void defaultsToDisconnectedAppearance() {
        StationBlock s;
        QVERIFY(!s.isConnectedAppearance());
        QVERIFY(s.radioName().isEmpty());
    }

    void setRadioNameSwitchesToConnected() {
        StationBlock s;
        s.setRadioName(QStringLiteral("ANAN-G2 (Saturn)"));
        QVERIFY(s.isConnectedAppearance());
        QCOMPARE(s.radioName(), QStringLiteral("ANAN-G2 (Saturn)"));
        // Cyan-border style applied
        QVERIFY(s.styleSheet().contains(QStringLiteral("rgba(0,180,216,80)")));
    }

    void emptyNameRevertsToDisconnected() {
        StationBlock s;
        s.setRadioName(QStringLiteral("X"));
        s.setRadioName(QString());
        QVERIFY(!s.isConnectedAppearance());
        // Dashed red style applied
        QVERIFY(s.styleSheet().contains(QStringLiteral("dashed")));
    }

    void leftClickEmitsClickedInBothAppearances() {
        StationBlock s;
        s.resize(180, 22);
        QSignalSpy spy(&s, &StationBlock::clicked);

        // Disconnected
        QTest::mouseClick(&s, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);

        // Connected
        s.setRadioName(QStringLiteral("ANAN-G2"));
        QTest::mouseClick(&s, Qt::LeftButton);
        QCOMPARE(spy.count(), 2);
    }

    void rightClickEmitsContextMenuOnlyWhenConnected() {
        StationBlock s;
        s.resize(180, 22);
        QSignalSpy spy(&s, &StationBlock::contextMenuRequested);

        // Disconnected — right-click should NOT emit
        QTest::mouseClick(&s, Qt::RightButton);
        QCOMPARE(spy.count(), 0);

        // Connected — right-click SHOULD emit
        s.setRadioName(QStringLiteral("ANAN-G2"));
        QTest::mouseClick(&s, Qt::RightButton);
        QCOMPARE(spy.count(), 1);
    }

    void hardwareLineJoinsModelAndFirmware() {
        StationBlock b;
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        QCOMPARE(b.hardwareLine(), QStringLiteral("ANAN-G2 · v27"));
    }

    void modelAloneOmitsTheSeparator() {
        StationBlock b;
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QString());
        QCOMPARE(b.hardwareLine(), QStringLiteral("ANAN-G2"));
    }

    void firmwareAloneOmitsTheSeparator() {
        StationBlock b;
        b.setHardwareLine(QString(), QStringLiteral("v27"));
        QCOMPARE(b.hardwareLine(), QStringLiteral("v27"));
    }

    void bothEmptyGivesAnEmptyLine() {
        StationBlock b;
        // Seed a non-default state first. m_hardwareLine starts empty by
        // default construction, so asserting empty afterward with no seed
        // would still pass even if setHardwareLine()'s entire body were
        // deleted (final-fix-wave finding 9) -- it would just be
        // re-confirming the untouched default.
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        QVERIFY(!b.hardwareLine().isEmpty());
        b.setHardwareLine(QString(), QString());
        QCOMPARE(b.hardwareLine(), QString());
    }

    // Both rows must centre. The name label predated the hardware row and
    // had no alignment set, so it defaulted to left while the row beneath
    // it was centred. With the hardware line wider than the name, the name
    // hung visibly left inside the box (bench report 2026-08-03). Nothing
    // asserted alignment before, which is how it shipped.
    void bothRowsAreCentred() {
        StationBlock b;
        b.setRadioName(QStringLiteral("ANAN-G2E"));
        b.setHardwareLine(QStringLiteral("HermesC10"), QStringLiteral("v110"));

        const QList<QLabel*> labels = b.findChildren<QLabel*>();
        QVERIFY2(labels.size() >= 2, "expected a name row and a hardware row");
        for (QLabel* l : labels) {
            QVERIFY2(l->alignment().testFlag(Qt::AlignHCenter),
                     qPrintable(QStringLiteral("row not centred: '%1'")
                                    .arg(l->text())));
        }
    }

    void clearingTheNameAlsoClearsTheHardwareLine() {
        StationBlock b;
        b.setRadioName(QStringLiteral("Nereus G2"));
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        b.setRadioName(QString());
        QCOMPARE(b.hardwareLine(), QString());
        QVERIFY(!b.isConnectedAppearance());
    }
};

QTEST_MAIN(TstStationBlock)
#include "tst_station_block.moc"
