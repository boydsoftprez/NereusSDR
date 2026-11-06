// no-port-check: NereusSDR-original test of the Linux platform theme choice.
// Modification history (NereusSDR):
//   2026-11-05 - Issue 700: the Cinnamon portal theme decision. J.J. Boyd
//                 (KG4VCF), AI-assisted via Claude Code.
#include <QtTest>

#include "gui/PortalThemeChoice.h"

using NereusSDR::shouldUsePortalThemeOnThisDesktop;

class TstPortalThemeChoice : public QObject
{
    Q_OBJECT

private slots:
    void choice_data()
    {
        QTest::addColumn<QString>("desktop");
        QTest::addColumn<QString>("theme");
        QTest::addColumn<bool>("expected");

        QTest::newRow("reporter's Cinnamon") << "X-Cinnamon" << "" << true;
        QTest::newRow("plain Cinnamon") << "Cinnamon" << "" << true;
        QTest::newRow("case differs") << "X-CINNAMON" << "" << true;
        QTest::newRow("colon-separated list") << "ubuntu:X-Cinnamon" << "" << true;
        QTest::newRow("user's theme stands") << "X-Cinnamon" << "gtk3" << false;
        QTest::newRow("KDE") << "KDE" << "" << false;
        QTest::newRow("GNOME") << "GNOME" << "" << false;
        QTest::newRow("no desktop") << "" << "" << false;
    }

    void choice()
    {
        QFETCH(QString, desktop);
        QFETCH(QString, theme);
        QFETCH(bool, expected);
        QCOMPARE(shouldUsePortalThemeOnThisDesktop(desktop, theme), expected);
    }
};

QTEST_APPLESS_MAIN(TstPortalThemeChoice)
#include "tst_portal_theme_choice.moc"
