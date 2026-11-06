// no-port-check: NereusSDR-original test of the Linux platform theme choice.
// Modification history (NereusSDR):
//   2026-11-06 - Issue 700: an AppImage keeps Qt's own dialog. J.J. Boyd
//                 (KG4VCF), AI-assisted via Claude Code.
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
        QTest::addColumn<bool>("inAppImage");
        QTest::addColumn<bool>("expected");

        QTest::newRow("reporter's Cinnamon") << "X-Cinnamon" << "" << false << true;
        QTest::newRow("plain Cinnamon") << "Cinnamon" << "" << false << true;
        QTest::newRow("case differs") << "X-CINNAMON" << "" << false << true;
        QTest::newRow("colon-separated list") << "ubuntu:X-Cinnamon" << "" << false << true;
        QTest::newRow("user's theme stands") << "X-Cinnamon" << "gtk3" << false << false;
        QTest::newRow("KDE") << "KDE" << "" << false << false;
        QTest::newRow("GNOME") << "GNOME" << "" << false << false;
        QTest::newRow("no desktop") << "" << "" << false << false;
        QTest::newRow("AppImage on Cinnamon") << "X-Cinnamon" << "" << true << false;
        QTest::newRow("AppImage on Cinnamon in a list") << "ubuntu:X-Cinnamon" << "" << true << false;
        QTest::newRow("AppImage with the user's theme") << "X-Cinnamon" << "gtk3" << true << false;
        QTest::newRow("AppImage on KDE") << "KDE" << "" << true << false;
    }

    void choice()
    {
        QFETCH(QString, desktop);
        QFETCH(QString, theme);
        QFETCH(bool, inAppImage);
        QFETCH(bool, expected);
        QCOMPARE(shouldUsePortalThemeOnThisDesktop(desktop, theme, inAppImage), expected);
    }
};

QTEST_APPLESS_MAIN(TstPortalThemeChoice)
#include "tst_portal_theme_choice.moc"
