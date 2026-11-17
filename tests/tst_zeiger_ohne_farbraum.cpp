// tests/tst_zeiger_ohne_farbraum.cpp  (Longpath)
// no-port-check: Longpath-original test file.
//
// Absturz 2026-11-10 18:15: der Bewegen-Zeiger (Qt::SizeAllCursor) im
// Bandbreitenfeld. Qt baut ihn auf macOS aus einem Bild mit eingebettetem
// Monitorprofil, und Qt 6.11.0/6.11.1 gibt beim Umwandeln eines Bildes mit
// eigenem Profil den Farbraum zu frueh frei (QTBUG-147602). Geprueft wird:
//
//   1. zeigerAusBildOhneFarbraum legt das Profil ab, laesst aber Bild,
//      Groesse und Griffpunkt, wie sie sind;
//   2. was bewegenZeiger() liefert, traegt kein eigenes Profil;
//   3. im Quelltext steht keine Zeigerform mehr direkt, die Qt auf macOS
//      aus einem Bild mit Profil baut (SizeAll, Wait, Busy) -- sonst kaeme
//      der Absturz mit der naechsten Stelle zurueck.

#include <QtTest>

#include <QColorSpace>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

#include "gui/ZeigerOhneFarbraum.h"

using namespace Longpath;

class TstZeigerOhneFarbraum : public QObject
{
    Q_OBJECT

private slots:
    void derZeigerTraegtKeinFarbprofil()
    {
        // Ein Bild wie Qts sizeallcursor.png: 16x16, mit eigenem Profil
        // (dort "SyncMaster Calibrated", hier ein Adobe-RGB mit Gamma 1,8).
        QImage bild(16, 16, QImage::Format_ARGB32_Premultiplied);
        bild.fill(Qt::transparent);
        for (int i = 0; i < 16; ++i) {
            bild.setPixel(i, 8, qRgba(0, 0, 0, 255));
            bild.setPixel(8, i, qRgba(255, 255, 255, 255));
        }
        bild.setColorSpace(QColorSpace(QColorSpace::Primaries::AdobeRgb, 1.8f));
        QVERIFY(bild.colorSpace().isValid());
        QVERIFY(bild.colorSpace() != QColorSpace(QColorSpace::SRgb));

        const QCursor zeiger = zeigerAusBildOhneFarbraum(bild, QPoint(8, 8));
        QCOMPARE(zeiger.shape(), Qt::BitmapCursor);
        QCOMPARE(zeiger.hotSpot(), QPoint(8, 8));

        const QImage drin = zeiger.pixmap().toImage()
                                .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!drin.colorSpace().isValid(),
                 qPrintable(drin.colorSpace().description()));
        QCOMPARE(drin.size(), bild.size());
        // Nur das Profil ist weg, die Bildpunkte sind dieselben.
        QCOMPARE(drin.pixel(3, 8), bild.pixel(3, 8));
        QCOMPARE(drin.pixel(8, 3), bild.pixel(8, 3));
        QCOMPARE(drin.pixel(1, 1), bild.pixel(1, 1));
    }

    void derBewegenZeigerIstSicher()
    {
        const QCursor zeiger = bewegenZeiger();
        if (zeiger.shape() == Qt::BitmapCursor) {
            QVERIFY(!zeiger.pixmap().toImage().colorSpace().isValid());
            QCOMPARE(zeiger.hotSpot(), QPoint(8, 8));
        } else {
            // Offscreen und ausserhalb von macOS: Qts Bild ist nicht
            // geladen bzw. das System hat einen eigenen Zeiger.
            QCOMPARE(zeiger.shape(), Qt::SizeAllCursor);
        }
    }

    void keinBildzeigerDirektImQuelltext()
    {
        const QString wurzel = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
        QVERIFY2(!wurzel.isEmpty(), "LONGPATH_SOURCE_DIR fehlt -- ueber ctest fahren");
        const QRegularExpression muster(QStringLiteral(R"(Qt::(SizeAllCursor|WaitCursor|BusyCursor)\b)"));
        QStringList funde;
        int dateien = 0;
        QDirIterator it(wurzel + QStringLiteral("/src"),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.mm")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            if (pfad.contains(QLatin1String("/ZeigerOhneFarbraum."))) { continue; }
            QFile datei(pfad);
            if (!datei.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            ++dateien;
            int zeile = 0;
            for (const QByteArray& roh : datei.readAll().split('\n')) {
                ++zeile;
                const QString text = QString::fromUtf8(roh).trimmed();
                if (text.startsWith(QLatin1String("//"))) { continue; }
                if (muster.match(text).hasMatch()) {
                    funde << QStringLiteral("%1:%2: %3")
                                 .arg(QDir(wurzel).relativeFilePath(pfad)).arg(zeile).arg(text);
                }
            }
        }
        QVERIFY2(dateien > 100, qPrintable(QStringLiteral("nur %1 Dateien gelesen").arg(dateien)));
        QVERIFY2(funde.isEmpty(),
                 qPrintable(QStringLiteral("Bildzeiger direkt gesetzt -- bewegenZeiger() nehmen:\n")
                            + funde.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TstZeigerOhneFarbraum)
#include "tst_zeiger_ohne_farbraum.moc"
