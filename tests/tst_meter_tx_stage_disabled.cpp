// =================================================================
// tests/tst_meter_tx_stage_disabled.cpp  (NereusSDR)
// =================================================================
//
// Container item parity Task 7: a TX-stage bar dims while its stage is off
// (Thetis DisableMeterType driven by OnEQChanged, OnLevelerChanged,
// OnCFCChanged and OnCompandChanged, MeterManager.cs:5313-5366 and
// fade(), MeterManager.cs:33153-33177 [v2.10.3.15]).
//
// Modification history (NereusSDR):
//   2026-10-26 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test; cites Thetis behavior, ports no logic.

#include <QtTest>
#include <QSignalSpy>

#include "gui/containers/ContentFade.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "models/TransmitModel.h"

#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

std::unique_ptr<BarPresetItem> makeBar(const QString& flavor)
{
    auto bar = std::make_unique<BarPresetItem>();
    if (!bar->configureVariant(flavor)) { return nullptr; }
    return bar;
}

// Steps `item`'s fade from `startMs` to `endMs` in 100 ms ticks (the
// default update interval) and returns the value at the end.
int fadeAt(MeterItem& item, qint64 startMs, qint64 endMs)
{
    for (qint64 t = startMs; t <= endMs; t += 100) { item.advanceFade(t); }
    return item.fadeValue();
}

void setAllStages(TransmitModel& tx, bool on)
{
    tx.setTxEqEnabled(on);
    tx.setTxLevelerOn(on);
    tx.setCfcEnabled(on);
    tx.setCpdrOn(on);
}

} // namespace

class TstMeterTxStageDisabled : public QObject {
    Q_OBJECT

    std::vector<std::unique_ptr<BarPresetItem>> m_bars;
    QVector<MeterItem*> m_items;

    BarPresetItem* bar(const QString& flavor) const
    {
        for (const auto& b : m_bars) {
            if (b->typeId() == QStringLiteral("meter.") + flavor.at(0).toLower() + flavor.mid(1)) { return b.get(); }
        }
        return nullptr;
    }

    // The MainWindow wiring: each of the four stage signals re-applies the
    // state to every meter item.
    void wire(TransmitModel& tx)
    {
        const auto apply = [this, &tx]() { applyTxStageState(m_items, tx); };
        connect(&tx, &TransmitModel::txEqEnabledChanged, this, apply);
        connect(&tx, &TransmitModel::txLevelerOnChanged, this, apply);
        connect(&tx, &TransmitModel::cfcEnabledChanged, this, apply);
        connect(&tx, &TransmitModel::cpdrOnChanged, this, apply);
    }

    void expectDisabled(const QStringList& flavors)
    {
        for (const auto& b : m_bars) {
            const bool expected = flavors.contains(b->typeId());
            QVERIFY2(b->stageDisabled() == expected, qPrintable(b->typeId()));
        }
    }

private slots:
    void init()
    {
        m_bars.clear();
        m_items.clear();
        for (const QString& f : {QStringLiteral("Eq"), QStringLiteral("Leveler"), QStringLiteral("LevelerGain"),
                                 QStringLiteral("Cfc"), QStringLiteral("CfcGain"), QStringLiteral("Comp"),
                                 QStringLiteral("Alc"), QStringLiteral("AlcGain"), QStringLiteral("Signal"),
                                 QStringLiteral("Pwr")}) {
            // Alc is not a configureVariant flavor; configureAsAlc makes it.
            std::unique_ptr<BarPresetItem> b;
            if (f == QStringLiteral("Alc")) {
                b = std::make_unique<BarPresetItem>();
                b->configureAsAlc();
            } else {
                b = makeBar(f);
            }
            QVERIFY2(b, qPrintable(f));
            m_items.append(b.get());
            m_bars.push_back(std::move(b));
        }
    }

    void stageMap()
    {
        QCOMPARE(bar("Eq")->txStage(), TxStage::Eq);
        QCOMPARE(bar("Leveler")->txStage(), TxStage::Leveler);
        QCOMPARE(bar("LevelerGain")->txStage(), TxStage::Leveler);
        QCOMPARE(bar("Cfc")->txStage(), TxStage::Cfc);
        QCOMPARE(bar("CfcGain")->txStage(), TxStage::Cfc);
        QCOMPARE(bar("Comp")->txStage(), TxStage::Compander);
        QCOMPARE(bar("AlcGain")->txStage(), TxStage::None);
        QCOMPARE(bar("Signal")->txStage(), TxStage::None);
        QCOMPARE(bar("Pwr")->txStage(), TxStage::None);
        for (const auto& b : m_bars) {
            if (b->typeId() == QStringLiteral("meter.alc")) { QCOMPARE(b->txStage(), TxStage::None); }
        }
    }

    void initialState()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        applyTxStageState(m_items, tx);
        expectDisabled({});
        setAllStages(tx, false);
        applyTxStageState(m_items, tx);
        expectDisabled({"meter.eq", "meter.leveler", "meter.levelerGain", "meter.cfc", "meter.cfcGain", "meter.comp"});
    }

    void eqSignal()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        applyTxStageState(m_items, tx);
        wire(tx);
        tx.setTxEqEnabled(false);
        expectDisabled({"meter.eq"});
        tx.setTxEqEnabled(true);
        expectDisabled({});
    }

    void levelerSignal()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        applyTxStageState(m_items, tx);
        wire(tx);
        tx.setTxLevelerOn(false);
        expectDisabled({"meter.leveler", "meter.levelerGain"});
        tx.setTxLevelerOn(true);
        expectDisabled({});
    }

    void cfcSignal()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        applyTxStageState(m_items, tx);
        wire(tx);
        tx.setCfcEnabled(false);
        expectDisabled({"meter.cfc", "meter.cfcGain"});
        tx.setCfcEnabled(true);
        expectDisabled({});
    }

    void companderSignal()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        applyTxStageState(m_items, tx);
        wire(tx);
        tx.setCpdrOn(false);
        expectDisabled({"meter.comp"});
        tx.setCpdrOn(true);
        expectDisabled({});
    }

    // EQ off: the bar steps down to 48 over 500 ms whatever its fade
    // options; EQ on: it recovers to 255.
    void eqOffDimsAndRecovers()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        wire(tx);
        BarPresetItem* eq = bar("Eq");
        QVERIFY(!eq->fadeOnRx());
        QVERIFY(!eq->fadeOnTx());
        eq->advanceFade(0);
        QCOMPARE(fadeAt(*eq, 100, 1000), ContentFade::kUndimmedLevel);

        tx.setTxEqEnabled(false);
        // At rest from 1000 ms; steps of ceil(207 / 5) = 42 each 100 ms.
        eq->advanceFade(1000);
        QCOMPARE(fadeAt(*eq, 1100, 1400), ContentFade::kUndimmedLevel - 4 * 42);
        QCOMPARE(fadeAt(*eq, 1500, 1500), ContentFade::kDimmedLevel);
        QCOMPARE(fadeAt(*eq, 1600, 2000), ContentFade::kDimmedLevel);
        // Transmitting does not lift it.
        eq->setTransmitting(true);
        QCOMPARE(fadeAt(*eq, 2100, 2500), ContentFade::kDimmedLevel);
        eq->setTransmitting(false);

        tx.setTxEqEnabled(true);
        QCOMPARE(fadeAt(*eq, 2600, 3000), ContentFade::kUndimmedLevel);

        // Another bar never moved.
        BarPresetItem* leveler = bar("Leveler");
        leveler->advanceFade(0);
        QCOMPARE(fadeAt(*leveler, 100, 3000), ContentFade::kUndimmedLevel);
    }

    // A matching bar added while the stage is off starts dimming at once.
    void addedWhileOff()
    {
        TransmitModel tx;
        setAllStages(tx, true);
        tx.setCfcEnabled(false);
        std::unique_ptr<BarPresetItem> added = makeBar(QStringLiteral("CfcGain"));
        QVERIFY(added);
        // onContainerItemAdded's call.
        applyTxStageState({added.get()}, tx);
        QVERIFY(added->stageDisabled());
        added->advanceFade(0);
        QCOMPARE(fadeAt(*added, 100, 500), ContentFade::kDimmedLevel);
    }

    // A foreign session's entry is never dimmed by this window's stages.
    void foreignSessionNotDimmed()
    {
        TransmitModel tx;
        setAllStages(tx, false);
        BarPresetItem* comp = bar("Comp");
        comp->setForeignSession(true);
        applyTxStageState(m_items, tx);
        comp->advanceFade(0);
        QCOMPARE(fadeAt(*comp, 100, 1000), ContentFade::kUndimmedLevel);
        // Its own Fade on RX still applies.
        comp->setFadeOnRx(true);
        QCOMPARE(fadeAt(*comp, 1100, 1500), ContentFade::kDimmedLevel);
    }

    // Applying the state only reads the transmit model.
    void neverWritesTransmitModel()
    {
        TransmitModel tx;
        setAllStages(tx, false);
        QSignalSpy eq(&tx, &TransmitModel::txEqEnabledChanged);
        QSignalSpy lev(&tx, &TransmitModel::txLevelerOnChanged);
        QSignalSpy cfc(&tx, &TransmitModel::cfcEnabledChanged);
        QSignalSpy cpdr(&tx, &TransmitModel::cpdrOnChanged);
        applyTxStageState(m_items, tx);
        applyTxStageState(m_items, tx);
        QCOMPARE(eq.count() + lev.count() + cfc.count() + cpdr.count(), 0);
        QVERIFY(!tx.txEqEnabled());
        QVERIFY(!tx.txLevelerOn());
        QVERIFY(!tx.cfcEnabled());
        QVERIFY(!tx.cpdrOn());
    }
};

QTEST_MAIN(TstMeterTxStageDisabled)
#include "tst_meter_tx_stage_disabled.moc"
