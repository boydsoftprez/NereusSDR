// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Real DXCC entity callsigns (W1AW, CT1ABC) used as
// fixtures to exercise the 4-tier color-resolution chain. Precedent:
// B2-B6, C1-C3.
//
// NereusSDR - DxccColorProvider tests
//
// Phase 3J-2 Task C4. Pins the contract that DxccColorProvider
// integrates CtyDatParser + DxccWorkedStatus + AdifParser to resolve
// spot callsigns to a 4-tier QColor (NewDxcc / NewBand / NewMode /
// Worked) plus a default unknown color. Six tests:
//   - initialStateReturnsDefaultColor: before any cty.dat load,
//     colorForSpot returns an invalid QColor (no cty.dat means no
//     primary-prefix resolution, so statusForSpot is Unknown and
//     colorForSpot returns the default-constructed QColor sentinel).
//   - loadsCtyDatAndAdif: loadCtyDat() against the repo-root
//     cty.dat returns true; importAdifFile() against the bundled
//     10-QSO sample fixture emits importFinished(qsoCount,
//     entityCount) with the expected counts (10 QSOs, 5 entities).
//   - resolvesNewDxccColor: a callsign in an entity not present
//     in the fixture (CT1ABC = Portugal) resolves to the
//     bright-red colorNewDxcc.
//   - resolvesNewBandColor: W1AW (K=USA, worked on 20m + 40m)
//     queried on 15m PHONE resolves to the orange colorNewBand.
//   - resolvesNewModeColor: W1AW worked on 40m PHONE but never
//     40m CW; querying 7.050 MHz with explicit mode "CW"
//     resolves to the gold colorNewMode.
//   - resolvesWorkedColor: W1AW worked on 20m PHONE; querying
//     14.250 MHz USB resolves to the dim-grey colorWorked.

#include <QtTest>
#include <QSignalSpy>
#include <QFileInfo>

#include "core/DxccColorProvider.h"

using namespace NereusSDR;

// Path resolvers mirror C1 / C2 conventions: the cty.dat lives at
// the worktree root; the ADIF sample fixture lives at
// tests/fixtures/adif/sample.adi. __FILE__ resolves to
// .../tests/tst_dxcc_color_provider.cpp inside the source tree.
static QString resolveCtyDatPath()
{
    const QString file = QString::fromUtf8(__FILE__);
    // __FILE__ = .../<worktree>/tests/tst_dxcc_color_provider.cpp
    // strip the last two components -> .../<worktree>/
    const QString root = QFileInfo(QFileInfo(file).dir().path()).path();
    return root + "/cty.dat";
}

static QString resolveAdifPath()
{
    const QString file = QString::fromUtf8(__FILE__);
    const QString testsDir = QFileInfo(file).dir().path();
    return testsDir + "/fixtures/adif/sample.adi";
}

class TestDxccColorProvider : public QObject {
    Q_OBJECT
private slots:
    void initialStateReturnsDefaultColor();
    void loadsCtyDatAndAdif();
    void resolvesNewDxccColor();
    void resolvesNewBandColor();
    void resolvesNewModeColor();
    void resolvesWorkedColor();
};

void TestDxccColorProvider::initialStateReturnsDefaultColor()
{
    DxccColorProvider provider;
    provider.setEnabled(true);
    // Before any cty.dat load, primary-prefix resolution returns
    // empty -> statusForSpot is Unknown -> colorForSpot returns a
    // default-constructed QColor (no spot-color override).
    const QColor c = provider.colorForSpot("W1AW", 14.250, "USB");
    QVERIFY(!c.isValid());
}

void TestDxccColorProvider::loadsCtyDatAndAdif()
{
    DxccColorProvider provider;
    QVERIFY2(provider.loadCtyDat(resolveCtyDatPath()),
             qPrintable(QString("cty.dat load failed: %1").arg(resolveCtyDatPath())));

    QSignalSpy spy(&provider, &DxccColorProvider::importFinished);
    provider.importAdifFile(resolveAdifPath());
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    const int qsoCount    = spy.first()[0].toInt();
    const int entityCount = spy.first()[1].toInt();
    QCOMPARE(qsoCount,    10);   // 10 QSOs in sample.adi
    QCOMPARE(entityCount, 5);    // K, JA, VK, G, DL
}

void TestDxccColorProvider::resolvesNewDxccColor()
{
    DxccColorProvider provider;
    QVERIFY(provider.loadCtyDat(resolveCtyDatPath()));

    QSignalSpy spy(&provider, &DxccColorProvider::importFinished);
    provider.importAdifFile(resolveAdifPath());
    QVERIFY(spy.wait(5000));

    provider.setEnabled(true);

    // CT1ABC (Portugal) is NOT in the fixture, so the entity is
    // never worked -> NewDxcc -> bright-red.
    const QColor c = provider.colorForSpot("CT1ABC", 14.250, "USB");
    QCOMPARE(c, provider.colorNewDxcc);
}

void TestDxccColorProvider::resolvesNewBandColor()
{
    DxccColorProvider provider;
    QVERIFY(provider.loadCtyDat(resolveCtyDatPath()));

    QSignalSpy spy(&provider, &DxccColorProvider::importFinished);
    provider.importAdifFile(resolveAdifPath());
    QVERIFY(spy.wait(5000));

    provider.setEnabled(true);

    // W1AW (K=USA) is worked on 20m + 40m, never 15m. 21.275 MHz
    // USB -> 15m PHONE -> NewBand -> orange.
    const QColor c = provider.colorForSpot("W1AW", 21.275, "USB");
    QCOMPARE(c, provider.colorNewBand);
}

void TestDxccColorProvider::resolvesNewModeColor()
{
    DxccColorProvider provider;
    QVERIFY(provider.loadCtyDat(resolveCtyDatPath()));

    QSignalSpy spy(&provider, &DxccColorProvider::importFinished);
    provider.importAdifFile(resolveAdifPath());
    QVERIFY(spy.wait(5000));

    provider.setEnabled(true);

    // W1AW worked on 40m PHONE only (third record in fixture).
    // 7.050 MHz CW -> 40m CW -> NewMode -> gold. Note: explicit
    // "CW" mode bypasses the band-plan inference path so the
    // 7.040-7.100 DATA segment doesn't apply.
    const QColor c = provider.colorForSpot("W1AW", 7.050, "CW");
    QCOMPARE(c, provider.colorNewMode);
}

void TestDxccColorProvider::resolvesWorkedColor()
{
    DxccColorProvider provider;
    QVERIFY(provider.loadCtyDat(resolveCtyDatPath()));

    QSignalSpy spy(&provider, &DxccColorProvider::importFinished);
    provider.importAdifFile(resolveAdifPath());
    QVERIFY(spy.wait(5000));

    provider.setEnabled(true);

    // W1AW worked on 20m PHONE (first record in fixture).
    // 14.250 MHz USB -> 20m PHONE -> Worked -> dim-grey.
    const QColor c = provider.colorForSpot("W1AW", 14.250, "USB");
    QCOMPARE(c, provider.colorWorked);
}

QTEST_GUILESS_MAIN(TestDxccColorProvider)
#include "tst_dxcc_color_provider.moc"
