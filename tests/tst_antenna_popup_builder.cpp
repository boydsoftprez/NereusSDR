// no-port-check: NereusSDR-original test file for AntennaPopupBuilder utility.
//
// AntennaPopupBuilder consolidates capability-gated antenna popup construction
// across VfoWidget, RxApplet, and SpectrumOverlayPanel. This test verifies the
// three radio tiers (full Alex / mid Alex / no Alex) and RX-vs-TX mode filtering.
//
// Per docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B3.
//
// Modification history (NereusSDR):
//   2026-09-27: "RX out on TX" follows the VFO flag's BYPS gate (the relay
//               and the product's RX out on TX), so a G2 and an HL2 offer
//               it nowhere and an ANAN-100D offers it in both places
//               (R-IOS-06, R-IOS-27). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.

#include <QMenu>
#include <QApplication>
#include <QtTest/QtTest>

#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
#include "gui/AntennaPopupBuilder.h"

using namespace NereusSDR;

class TstAntennaPopupBuilder : public QObject {
    Q_OBJECT
private slots:
    void tier1FullAlex_rxMode_showsAllSections();
    void tier2MidAlex_rxMode_hidesRxOnlyAndBypass();
    void tier3NoAlex_emitsSingleAntOrEmpty();
    void txMode_filtersOutRxOnlySection();
    void labels_roundTripsMenu();
    void rxOutOnTxFollowsTheBypsGate_data();
    void rxOutOnTxFollowsTheBypsGate();
};

// Tier 1: Full-Alex board with bypass + 3 RX-only ports, on a product that
// offers RX out on TX (the ANAN-100D).
// In RX mode, should show Main TX/RX (ANT1-3) + RX-only (3 ports by sku) +
// Special (RX out on TX bypass) sections.
void TstAntennaPopupBuilder::tier1FullAlex_rxMode_showsAllSections()
{
    BoardCapabilities caps{};
    caps.hasAlex           = true;
    caps.hasAlex2          = true;
    caps.antennaInputCount = 3;
    caps.rxOnlyAntennaCount = 3;
    caps.hasRxBypassRelay  = true;

    // ANAN-100D sku: rxOnlyLabels = {EXT2, EXT1, XVTR}, hasRxOutOnTx = true
    // (with the relay above, the BYPS gate holds).
    const SkuUiProfile sku = skuUiProfileFor(HPSDRModel::ANAN100D);

    QMenu menu;
    AntennaPopupBuilder::populate(&menu, caps, sku,
        AntennaPopupBuilder::Mode::RX, QStringLiteral("ANT1"));

    // Should contain: ANT1, ANT2, ANT3 + EXT2, EXT1, XVTR + RX out on TX
    // plus up to 3 separator actions — total non-separator actions >= 7
    int actionCount = 0;
    bool foundAnt1 = false, foundAnt2 = false, foundAnt3 = false;
    bool foundRxOnly = false;
    bool foundBypass = false;
    for (QAction* a : menu.actions()) {
        if (a->isSeparator()) { continue; }
        ++actionCount;
        const QString t = a->text();
        if (t == QStringLiteral("ANT1"))             { foundAnt1 = true; }
        if (t == QStringLiteral("ANT2"))             { foundAnt2 = true; }
        if (t == QStringLiteral("ANT3"))             { foundAnt3 = true; }
        if (t == QStringLiteral("EXT2") ||
            t == QStringLiteral("EXT1") ||
            t == QStringLiteral("XVTR"))             { foundRxOnly = true; }
        if (t == QStringLiteral("RX out on TX"))     { foundBypass = true; }
    }
    QVERIFY2(actionCount >= 7, qPrintable(
        QStringLiteral("Expected >= 7 non-separator actions, got %1").arg(actionCount)));
    QVERIFY(foundAnt1);
    QVERIFY(foundAnt2);
    QVERIFY(foundAnt3);
    QVERIFY(foundRxOnly);
    QVERIFY(foundBypass);
}

// Tier 2: Mid-Alex board (Hermes/ANAN100 family) with EXT2/EXT1/XVTR RX-only
// but no bypass relay.  In RX mode: ANT1-3 + RX-only section — but no bypass.
void TstAntennaPopupBuilder::tier2MidAlex_rxMode_hidesRxOnlyAndBypass()
{
    BoardCapabilities caps{};
    caps.hasAlex            = true;
    caps.hasAlex2           = false;
    caps.antennaInputCount  = 3;
    caps.rxOnlyAntennaCount = 3;
    caps.hasRxBypassRelay   = false;

    // ANAN100 sku: rxOnlyLabels = {EXT2, EXT1, XVTR}, hasRxBypassUi = false
    const SkuUiProfile sku = skuUiProfileFor(HPSDRModel::ANAN100);

    QMenu menu;
    AntennaPopupBuilder::populate(&menu, caps, sku,
        AntennaPopupBuilder::Mode::RX, QStringLiteral("ANT1"));

    bool foundBypass = false;
    int rxOnlyCount = 0;
    for (QAction* a : menu.actions()) {
        if (a->isSeparator()) { continue; }
        const QString t = a->text();
        if (t == QStringLiteral("RX out on TX")) { foundBypass = true; }
        if (t == QStringLiteral("EXT1") || t == QStringLiteral("EXT2") ||
            t == QStringLiteral("XVTR")) { ++rxOnlyCount; }
    }
    QVERIFY2(!foundBypass, "Bypass should not appear when hasRxBypassRelay=false");
    // EXT1/EXT2/XVTR *should* appear in RX mode when rxOnlyAntennaCount=3
    QVERIFY2(rxOnlyCount >= 1, qPrintable(
        QStringLiteral("Expected RX-only labels for ANAN100 sku, got %1").arg(rxOnlyCount)));
}

// Tier 3: No-Alex board (HL2). Should produce an empty popup (or at most
// one ANT1 fallback if antennaInputCount >= 1 on a hasAlex=false board).
// HL2 has hasAlex=false; per the codebase antennaInputCount=1 is still set.
// AntennaPopupBuilder with hasAlex=false + antennaInputCount<=0 → empty list.
void TstAntennaPopupBuilder::tier3NoAlex_emitsSingleAntOrEmpty()
{
    BoardCapabilities caps{};
    caps.hasAlex            = false;
    caps.antennaInputCount  = 1;
    caps.rxOnlyAntennaCount = 0;
    caps.hasRxBypassRelay   = false;

    const SkuUiProfile sku = skuUiProfileFor(HPSDRModel::HERMESLITE);

    QMenu menu;
    AntennaPopupBuilder::populate(&menu, caps, sku,
        AntennaPopupBuilder::Mode::RX, QStringLiteral("ANT1"));

    int nonSepActions = 0;
    for (QAction* a : menu.actions()) {
        if (!a->isSeparator()) { ++nonSepActions; }
    }
    QVERIFY2(nonSepActions <= 1,
             qPrintable(QStringLiteral(
                 "HL2 (no Alex) should show at most 1 action, got %1").arg(nonSepActions)));
}

// TX mode: RX-only section (EXT1/EXT2/XVTR) must not appear.
// Main ANT1-3 should still appear.
void TstAntennaPopupBuilder::txMode_filtersOutRxOnlySection()
{
    BoardCapabilities caps{};
    caps.hasAlex            = true;
    caps.hasAlex2           = true;
    caps.antennaInputCount  = 3;
    caps.rxOnlyAntennaCount = 3;
    caps.hasRxBypassRelay   = true;

    const SkuUiProfile sku = skuUiProfileFor(HPSDRModel::ANAN_G2);

    QMenu menu;
    AntennaPopupBuilder::populate(&menu, caps, sku,
        AntennaPopupBuilder::Mode::TX, QStringLiteral("ANT1"));

    bool foundRxOnly = false;
    bool foundAnt1 = false;
    for (QAction* a : menu.actions()) {
        if (a->isSeparator()) { continue; }
        const QString t = a->text();
        if (t == QStringLiteral("BYPS") || t == QStringLiteral("EXT1") ||
            t == QStringLiteral("XVTR") || t == QStringLiteral("RX out on TX")) {
            foundRxOnly = true;
        }
        if (t == QStringLiteral("ANT1")) { foundAnt1 = true; }
    }
    QVERIFY2(!foundRxOnly, "TX mode must not show RX-only or bypass entries");
    QVERIFY2(foundAnt1, "TX mode must still show main ANT entries");
}

// labels() convenience method should round-trip with populate():
// only returns non-separator data values.
void TstAntennaPopupBuilder::labels_roundTripsMenu()
{
    BoardCapabilities caps{};
    caps.hasAlex            = true;
    caps.antennaInputCount  = 2;
    caps.rxOnlyAntennaCount = 0;
    caps.hasRxBypassRelay   = false;

    const SkuUiProfile sku = skuUiProfileFor(HPSDRModel::HERMES);

    const QStringList lbls = AntennaPopupBuilder::labels(caps, sku,
        AntennaPopupBuilder::Mode::RX);

    QVERIFY2(lbls.size() >= 2,
             qPrintable(QStringLiteral("Expected >= 2 labels, got %1").arg(lbls.size())));
    QVERIFY(lbls.contains(QStringLiteral("ANT1")));
    QVERIFY(lbls.contains(QStringLiteral("ANT2")));
    // No separator entries
    for (const QString& l : lbls) {
        QVERIFY2(!l.isEmpty(), "labels() must not include empty/separator entries");
    }
}

// "RX out on TX" appears exactly where the VFO flag's BYPS button does: the
// board's bypass relay and the product's RX out on TX (VfoWidget's
// m_hasRxBypassRelay && m_hasRxOutOnTxUi), each radio's real values.
void TstAntennaPopupBuilder::rxOutOnTxFollowsTheBypsGate_data()
{
    QTest::addColumn<int>("model");
    QTest::addColumn<bool>("offered");
    // The G2 has the relay but not RX out on TX; the HL2 has neither.
    QTest::newRow("ANAN-G2") << static_cast<int>(HPSDRModel::ANAN_G2) << false;
    QTest::newRow("HL2") << static_cast<int>(HPSDRModel::HERMESLITE) << false;
    QTest::newRow("ANAN-7000DLE") << static_cast<int>(HPSDRModel::ANAN7000D) << false;
    QTest::newRow("ANAN-100D") << static_cast<int>(HPSDRModel::ANAN100D) << true;
    QTest::newRow("ANAN-200D") << static_cast<int>(HPSDRModel::ANAN200D) << true;
}

void TstAntennaPopupBuilder::rxOutOnTxFollowsTheBypsGate()
{
    QFETCH(int, model);
    QFETCH(bool, offered);
    const auto m = static_cast<HPSDRModel>(model);
    const BoardCapabilities& caps = BoardCapsTable::forModel(m);
    const SkuUiProfile sku = skuUiProfileFor(m);
    QCOMPARE(rxOutOnTxPresent(caps, sku), offered);
    QCOMPARE(caps.hasRxBypassRelay && sku.hasRxOutOnTx, offered);
    const QStringList rx = AntennaPopupBuilder::labels(caps, sku, AntennaPopupBuilder::Mode::RX);
    QCOMPARE(rx.contains(QStringLiteral("RX out on TX")), offered);
    const QStringList tx = AntennaPopupBuilder::labels(caps, sku, AntennaPopupBuilder::Mode::TX);
    QVERIFY(!tx.contains(QStringLiteral("RX out on TX")));
}

QTEST_MAIN(TstAntennaPopupBuilder)
#include "tst_antenna_popup_builder.moc"
