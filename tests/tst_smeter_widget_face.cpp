// tests/tst_smeter_widget_face.cpp
//
// Verifies the SMeterWidget meter-face selection (vintage faces drawn by
// gui/VintageMeterFace + the original Classic look):
//   - default face is Aged Cream, and constructing the widget does not write
//     SMeter_FaceStyle back to AppSettings
//   - setFaceStyle() persists SMeter_FaceStyle and a new widget restores it
//   - an unknown persisted value falls back to the default
//   - "Meter Face" is the 4th context submenu (index 3, after Peak Hold) with
//     one entry per FaceStyle, the current one checked
//   - every face renders: the card centre differs between a light and a dark
//     theme, and the pointer moves when the level changes
//   - no reading (R-R3-13): a level at or below -400 dBm, or a non-finite
//     one, reads "--" and "-- dBm" in every RX mode, the pointer rests at
//     the scale minimum and both peak markers clear; the next real level
//     (-130 dBm included) formats exactly as before
//
// Set NEREUS_SMETER_DUMP_DIR to a directory to write a PNG of every face
// (RX, TX power, and no reading) for eyeballing.
//
// NereusSDR-native test file; no upstream equivalent.

#include <QtTest>
#include <QAction>
#include <QImage>
#include <QMenu>

#include <limits>

#include "gui/SMeterWidget.h"
#include "gui/VintageMeterFace.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class SMeterWidgetFaceTest : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }
    void defaultIsAgedCreamAndNotPersistedByConstruction();
    void faceStylePersistsAndRestores();
    void unknownPersistedValueFallsBack();
    void menuHasMeterFaceSubmenu();
    void everyFaceRenders();
    void pointerMovesWithLevel();
    void noReadingShowsDashes_data();
    void noReadingShowsDashes();
    void realLevelAfterNoReadingDisplaysAsBefore();

private:
    static QImage grab(SMeterWidget& w, float needleSettleDbm);
};

QImage SMeterWidgetFaceTest::grab(SMeterWidget& w, float dbm)
{
    w.resize(300, 150);
    w.setLevel(dbm);
    // Let the needle ballistics settle (30 Hz animation timer).
    QTest::qWait(700);
    return w.grab().toImage();
}

void SMeterWidgetFaceTest::defaultIsAgedCreamAndNotPersistedByConstruction()
{
    SMeterWidget w;
    QCOMPARE(w.faceStyle(), SMeterWidget::FaceStyle::AgedCream);
    QVERIFY(!AppSettings::instance().contains("SMeter_FaceStyle"));
}

void SMeterWidgetFaceTest::faceStylePersistsAndRestores()
{
    {
        SMeterWidget w;
        w.setFaceStyle(SMeterWidget::FaceStyle::Blackface);
    }
    QCOMPARE(AppSettings::instance().value("SMeter_FaceStyle").toString(),
             QString("Blackface"));
    SMeterWidget restored;
    QCOMPARE(restored.faceStyle(), SMeterWidget::FaceStyle::Blackface);

    restored.setFaceStyle(SMeterWidget::FaceStyle::Classic);
    SMeterWidget classic;
    QCOMPARE(classic.faceStyle(), SMeterWidget::FaceStyle::Classic);
}

void SMeterWidgetFaceTest::unknownPersistedValueFallsBack()
{
    AppSettings::instance().setValue("SMeter_FaceStyle", QString("Chartreuse"));
    SMeterWidget w;
    QCOMPARE(w.faceStyle(), SMeterWidget::FaceStyle::AgedCream);
}

void SMeterWidgetFaceTest::menuHasMeterFaceSubmenu()
{
    SMeterWidget w;
    w.setFaceStyle(SMeterWidget::FaceStyle::CollinsWhite);
    QMenu* menu = w.buildContextMenuForTesting();
    const auto top = menu->actions();
    QCOMPARE(top.size(), 4);
    QCOMPARE(top.at(3)->text(), QString("Meter Face"));
    QMenu* faceMenu = top.at(3)->menu();
    QVERIFY(faceMenu != nullptr);

    QStringList labels;
    QString checked;
    for (QAction* a : faceMenu->actions()) {
        if (a->isSeparator()) { continue; }
        labels << a->text();
        if (a->isChecked()) { checked = a->text(); }
    }
    QCOMPARE(labels.size(), VintageMeterFace::themeCount() + 1);
    QCOMPARE(labels.first(), QString("Aged Cream"));
    QCOMPARE(labels.last(), QString("Classic (flat)"));
    QCOMPARE(checked, QString("Collins White"));

    // Triggering an entry switches the face.
    for (QAction* a : faceMenu->actions()) {
        if (a->text() == "Carbon") { a->trigger(); }
    }
    QCOMPARE(w.faceStyle(), SMeterWidget::FaceStyle::Carbon);
    menu->deleteLater();
}

void SMeterWidgetFaceTest::everyFaceRenders()
{
    const QString dumpDir = qEnvironmentVariable("NEREUS_SMETER_DUMP_DIR");
    // A point on the card clear of scale, lettering, pointer and the glass
    // highlight (which fades out from the top-left corner).
    const QPoint cardProbe(264, 62);

    QColor cream, black;
    for (int i = 0; i <= static_cast<int>(SMeterWidget::FaceStyle::Classic); ++i) {
        const auto style = static_cast<SMeterWidget::FaceStyle>(i);
        SMeterWidget w;
        w.setFaceStyle(style);
        const QImage rx = grab(w, -61.0f);   // S9+12
        QVERIFY(!rx.isNull());
        if (style == SMeterWidget::FaceStyle::AgedCream) { cream = rx.pixelColor(cardProbe); }
        if (style == SMeterWidget::FaceStyle::Blackface) { black = rx.pixelColor(cardProbe); }

        if (!dumpDir.isEmpty()) {
            w.resize(600, 300);
            rx.save(QString("%1/smeter_face_%2_rx.png").arg(dumpDir).arg(i));
            w.grab().save(QString("%1/smeter_face_%2_rx_large.png").arg(dumpDir).arg(i));
            w.resize(300, 150);
            w.setTransmitting(true);
            w.setTxMeters(87.0f, 1.3f);
            QTest::qWait(500);
            w.grab().save(QString("%1/smeter_face_%2_tx.png").arg(dumpDir).arg(i));
            w.setTransmitting(false);
            w.setLevel(-400.0f);
            QTRY_COMPARE(w.testNeedleFraction(), 0.0f);
            w.grab().save(QString("%1/smeter_face_%2_noreading.png").arg(dumpDir).arg(i));
        }
    }
    QVERIFY2(cream.lightness() > 170, qPrintable(cream.name()));
    QVERIFY2(black.lightness() < 60, qPrintable(black.name()));
}

void SMeterWidgetFaceTest::pointerMovesWithLevel()
{
    SMeterWidget w;
    const QImage low  = grab(w, -121.0f);   // S1
    const QImage high = grab(w, -33.0f);    // S9+40
    QVERIFY(low != high);
}

void SMeterWidgetFaceTest::noReadingShowsDashes_data()
{
    QTest::addColumn<QString>("rxMode");
    QTest::addColumn<float>("dbm");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const char* mode : {"Signal", "Sig Avg", "Signal Peak", "Max Bin"}) {
        QTest::addRow("%s, -400", mode) << QString(mode) << -400.0f;
        QTest::addRow("%s, -1000", mode) << QString(mode) << -1000.0f;
        QTest::addRow("%s, NaN", mode) << QString(mode) << nan;
        QTest::addRow("%s, -inf", mode) << QString(mode) << -inf;
        QTest::addRow("%s, +inf", mode) << QString(mode) << inf;
    }
}

void SMeterWidgetFaceTest::noReadingShowsDashes()
{
    QFETCH(QString, rxMode);
    QFETCH(float, dbm);

    SMeterWidget w;
    w.setRxMode(rxMode);
    // A real reading first: a -60 dBm peak held over a -100 dBm level, so the
    // peak hold line (and in Signal Peak mode the peak marker) is showing.
    w.setLevel(-60.0f);
    w.setLevel(-100.0f);
    QCOMPARE(w.testDbmReadout(),
             QString(rxMode == "Signal Peak" ? "-60 dBm" : "-100 dBm"));
    QVERIFY(w.testPeakMarkersShown());

    w.setLevel(dbm);
    QCOMPARE(w.testSUnitsReadout(), QString("--"));
    QCOMPARE(w.testDbmReadout(), QString("-- dBm"));
    QCOMPARE(w.sUnitsText(), QString("--"));
    QCOMPARE(w.testNeedleTarget(), 0.0f);
    QVERIFY(!w.testPeakMarkersShown());

    // Both paint paths draw the no-reading state.
    w.resize(300, 150);
    QVERIFY(!w.grab().isNull());
    w.setFaceStyle(SMeterWidget::FaceStyle::Classic);
    QVERIFY(!w.grab().isNull());
}

void SMeterWidgetFaceTest::realLevelAfterNoReadingDisplaysAsBefore()
{
    SMeterWidget w;   // Signal mode: init() cleared AppSettings

    // -130 dBm is below S0 and formats as it always has.
    w.setLevel(-130.0f);
    QCOMPARE(w.testSUnitsReadout(), QString("S0"));
    QCOMPARE(w.testDbmReadout(), QString("-130 dBm"));

    // Pointer up at S9, then no reading: it falls back and comes to rest at
    // the scale minimum.
    w.setLevel(-73.0f);
    QCOMPARE(w.testSUnitsReadout(), QString("S9"));
    QCOMPARE(w.testDbmReadout(), QString("-73 dBm"));
    QTRY_COMPARE(w.testNeedleFraction(), 0.6f);
    w.setLevel(-400.0f);
    QCOMPARE(w.testDbmReadout(), QString("-- dBm"));
    QCOMPARE(w.testNeedleTarget(), 0.0f);
    QTRY_COMPARE(w.testNeedleFraction(), 0.0f);

    // The next real level displays exactly as before.
    w.setLevel(-130.0f);
    QCOMPARE(w.testSUnitsReadout(), QString("S0"));
    QCOMPARE(w.testDbmReadout(), QString("-130 dBm"));
    QCOMPARE(w.sUnitsText(), QString("S0"));
    QCOMPARE(w.levelDbm(), -130.0f);

    // NaN, then a real level: readouts, pointer and peak hold all return.
    w.setLevel(std::numeric_limits<float>::quiet_NaN());
    QCOMPARE(w.testSUnitsReadout(), QString("--"));
    QCOMPARE(w.testDbmReadout(), QString("-- dBm"));
    QVERIFY(!w.testPeakMarkersShown());
    w.setLevel(-73.0f);
    QCOMPARE(w.testSUnitsReadout(), QString("S9"));
    QCOMPARE(w.testDbmReadout(), QString("-73 dBm"));
    QCOMPARE(w.testNeedleTarget(), 0.6f);
    QVERIFY(w.testPeakMarkersShown());
}

QTEST_MAIN(SMeterWidgetFaceTest)
#include "tst_smeter_widget_face.moc"
