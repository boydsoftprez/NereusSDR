// tests/tst_about_dialog.cpp
#include <QtTest/QtTest>
#include <QLabel>
#include <QPushButton>
#include "gui/AboutDialog.h"

using namespace NereusSDR;

class TstAboutDialog : public QObject
{
    Q_OBJECT

private slots:
    void dialogInstantiates()
    {
        AboutDialog dlg;
        QCOMPARE(dlg.windowTitle(), QStringLiteral("About NereusSDR"));
    }

    void hasAuthorCredit()
    {
        AboutDialog dlg;
        bool found = false;
        const auto labels = dlg.findChildren<QLabel*>();
        for (const auto* lbl : labels) {
            if (lbl->text().contains(QStringLiteral("JJ Boyd"))) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Expected a QLabel containing 'JJ Boyd'");
    }

    void hasContributorLinks()
    {
        AboutDialog dlg;
        const auto labels = dlg.findChildren<QLabel*>();

        QStringList expected = {
            QStringLiteral("ramdor/Thetis"),
            QStringLiteral("ten9876/AetherSDR"),
            QStringLiteral("mi0bot/OpenHPSDR-Thetis"),
            QStringLiteral("TAPR/OpenHPSDR-wdsp"),
            QStringLiteral("g0orx/wdsp"),
        };

        for (const auto& slug : expected) {
            bool found = false;
            for (const auto* lbl : labels) {
                if (lbl->text().contains(slug)) {
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, qPrintable(QStringLiteral("Missing link for %1").arg(slug)));
        }
    }

    void hasLibraryCards()
    {
        AboutDialog dlg;
        const auto labels = dlg.findChildren<QLabel*>();

        QStringList libs = {
            QStringLiteral("Qt 6"),
            QStringLiteral("FFTW3"),
            QStringLiteral("WDSP v1.29"),
        };

        for (const auto& lib : libs) {
            bool found = false;
            for (const auto* lbl : labels) {
                if (lbl->text().contains(lib)) {
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, qPrintable(QStringLiteral("Missing library card: %1").arg(lib)));
        }
    }

    void okButtonCloses()
    {
        AboutDialog dlg;
        auto* okBtn = dlg.findChild<QPushButton*>();
        QVERIFY(okBtn);
        QCOMPARE(okBtn->text(), QStringLiteral("OK"));

        QTest::mouseClick(okBtn, Qt::LeftButton);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }
};

QTEST_MAIN(TstAboutDialog)
#include "tst_about_dialog.moc"
