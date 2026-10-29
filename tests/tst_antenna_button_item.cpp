// =================================================================
// tests/tst_antenna_button_item.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test of the container antenna grid;
// it checks the ported on-state rules and the click routing, and ports no
// upstream logic itself.
//
// Container item parity, Task 50: the antenna grid follows its slice.
//   - On-state (Thetis setupButtons): a slice on ANT2 lights only Rx Ant 2;
//     a receive-only or XVTR input lights its own button and no Rx Ant;
//     Tx Ant n lights from the transmit antenna; Rx/Tx is never lit.
//   - Hiding a button removes it from the layout and the column range.
//   - The editor has one visibility row per button; Rx/Tx's row is
//     disabled with the not-built reason, and the button is never drawn.
//   - In a window: the receive buttons follow the entry's slice; a Tx Ant
//     click changes txAntenna on the transmit slice only and never keys
//     (no MOX, TUNE or key change); Rx/Tx changes nothing; an entry on
//     another session's radio changes nothing here.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 50.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QJsonObject>
#include <QLabel>
#include <QSignalSpy>

#include <array>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/TxSliceArbiter.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/meter_property_editors/AntennaButtonItemEditor.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/MeterWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// An ANAN-style radio's receive-only inputs (SkuUiProfile::rxOnlyLabels).
const std::array<QString, 3> kInputs = {QStringLiteral("BYPS"), QStringLiteral("EXT1"),
                                         QStringLiteral("XVTR")};

QList<int> litButtons(const AntennaButtonItem& grid)
{
    QList<int> lit;
    for (int i = 0; i < grid.buttonCount(); ++i) {
        if (grid.button(i).on) { lit << i; }
    }
    return lit;
}

SliceModel* ensureSlice(RadioModel* model)
{
    if (model->activeSlice() == nullptr) {
        model->addSlice();
        model->setActiveSlice(0);
    }
    return model->activeSlice();
}

} // namespace

class TstAntennaButtonItem : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("antenna-button-item-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("audio/FirstRunComplete"),
                                         QStringLiteral("True"));
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
        BuildIdentity::setBuildTag(QString());
    }

    void cleanup()
    {
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // Thetis MeterManager.cs:9721-9729: Rx Ant n lights from the receive
    // antenna while no receive-only input is in use.
    void sliceOnAnt2LightsOnlyRxAnt2()
    {
        AntennaButtonItem grid;
        grid.setAntennas(QStringLiteral("ANT2"), QStringLiteral("ANT1"), kInputs);
        QCOMPARE(litButtons(grid), (QList<int>{1, 6}));
        QCOMPARE(grid.activeRxAntenna(), 1);
        QCOMPARE(grid.activeTxAntenna(), 0);

        grid.setAntennas(QStringLiteral("ANT3"), QStringLiteral("ANT2"), kInputs);
        QCOMPARE(litButtons(grid), (QList<int>{2, 7}));
    }

    // Thetis MeterManager.cs:9717-9742: a receive-only input lights its
    // own button and no Rx Ant; XVTR lights button 5.
    void receiveOnlyInputsLightTheirOwnButton()
    {
        AntennaButtonItem grid;
        grid.setAntennas(QStringLiteral("BYPS"), QStringLiteral("ANT3"), kInputs);
        QCOMPARE(litButtons(grid), (QList<int>{3, 8}));
        grid.setAntennas(QStringLiteral("EXT1"), QStringLiteral("ANT3"), kInputs);
        QCOMPARE(litButtons(grid), (QList<int>{4, 8}));
        grid.setAntennas(QStringLiteral("XVTR"), QStringLiteral("ANT3"), kInputs);
        QCOMPARE(litButtons(grid), (QList<int>{AntennaButtonItem::kXvtrButton, 8}));

        // A Hermes names its inputs RX1 and RX2: the names, not positions.
        const std::array<QString, 3> hermes = {QStringLiteral("RX1"), QStringLiteral("RX2"),
                                               QStringLiteral("XVTR")};
        grid.setAntennas(QStringLiteral("RX2"), QStringLiteral("ANT1"), hermes);
        QCOMPARE(litButtons(grid), (QList<int>{4, 6}));
        // EXT1 is not one of a Hermes's inputs: nothing receive is lit.
        grid.setAntennas(QStringLiteral("EXT1"), QStringLiteral("ANT1"), hermes);
        QCOMPARE(litButtons(grid), (QList<int>{6}));

        // An unknown or empty antenna lights nothing; Rx/Tx never lights.
        grid.setAntennas(QString(), QString(), kInputs);
        QVERIFY(litButtons(grid).isEmpty());
        grid.setActiveRxAntenna(-1);
        grid.setActiveTxAntenna(-1);
        QVERIFY(litButtons(grid).isEmpty());
    }

    // Hiding a button takes it out of the layout: it is not drawn, the
    // shown count drops and the column range shrinks with it.
    void hidingAButtonRemovesItFromTheLayout()
    {
        AntennaButtonItem grid;
        QCOMPARE(grid.visibleBits(), 1023u);
        const int shown = grid.shownButtonCount();
        const auto range = grid.columnRange();
        QVERIFY(grid.isButtonShown(1));

        grid.setVisibleBits(grid.visibleBits() & ~(1u << 1));
        QVERIFY(!grid.isButtonShown(1));
        QCOMPARE(grid.shownButtonCount(), shown - 1);
        QCOMPARE(grid.columnRange(), std::make_pair(range.first, range.second - 1));

        // The saved bits come back as saved.
        AntennaButtonItem loaded;
        QVERIFY(loaded.deserialize(grid.serialize()));
        QCOMPARE(loaded.visibleBits(), grid.visibleBits());
        QVERIFY(!loaded.isButtonShown(1));
    }

    // One row per button; Rx/Tx (index 9) is never drawn while its feature
    // is not built, and its row says why.
    void editorShowsEachButtonAndRxTxReason()
    {
        QVERIFY(!UnbuiltFeatures::isBuilt(UnbuiltFeature::AntennaRxTxSplit));
        AntennaButtonItem grid;
        QVERIFY(!grid.isButtonShown(AntennaButtonItem::kRxTxButton));
        AntennaButtonItemEditor editor;
        editor.setItem(&grid);
        QSignalSpy changed(&editor, &BaseItemEditor::propertyChanged);
        for (int i = 0; i < AntennaButtonItem::kAntennaCount; ++i) {
            auto* check = editor.findChild<QCheckBox*>(QStringLiteral("antennaVisible_%1").arg(i));
            QVERIFY2(check, qPrintable(QString::number(i)));
            QVERIFY(check->isChecked());
        }
        auto* rxTx = editor.findChild<QCheckBox*>(QStringLiteral("antennaVisible_9"));
        QVERIFY(!rxTx->isEnabled());
        QCOMPARE(rxTx->toolTip(), UnbuiltFeatures::notBuiltReason());
        QVERIFY(!rxTx->toolTip().isEmpty());
        QVERIFY(editor.findChild<QCheckBox*>(QStringLiteral("antennaVisible_1"))->isEnabled());

        QCOMPARE(changed.count(), 0);
        editor.findChild<QCheckBox*>(QStringLiteral("antennaVisible_1"))->setChecked(false);
        QVERIFY(!grid.isButtonShown(1));
        QCOMPARE(changed.count(), 1);
        editor.findChild<QCheckBox*>(QStringLiteral("antennaVisible_1"))->setChecked(true);
        QVERIFY(grid.isButtonShown(1));
        QCOMPARE(grid.visibleBits(), 1023u);
    }

    // In a window: the grid follows its entry's slice; a Tx Ant click
    // writes the transmit slice only and never keys.
    void gridFollowsItsSliceAndTxClickNeverKeys()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* a = ensureSlice(model);
        QVERIFY(a != nullptr);
        model->addSlice();
        QCOMPARE(model->slices().size(), 2);
        SliceModel* b = model->slices().at(1);
        QVERIFY(b != nullptr && b != a);
        // Different bands: an antenna is kept per band.
        a->setFrequency(14074000.0);
        b->setFrequency(7074000.0);
        a->setRxAntenna(QStringLiteral("ANT1"));
        a->setTxAntenna(QStringLiteral("ANT1"));
        b->setRxAntenna(QStringLiteral("ANT1"));
        b->setTxAntenna(QStringLiteral("ANT1"));
        QCOMPARE(model->txBoundSlice(), a);

        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* ant = new AntennaButtonItem();
        meter->addItem(ant);
        container->wireInteractiveItem(ant);
        ant->setProperty("containerSourceContext", QJsonObject{{"sliceId", b->sliceIndex()}});

        QSignalSpy mox(&model->transmitModel(), &TransmitModel::moxChanged);
        QSignalSpy tune(&model->transmitModel(), &TransmitModel::tuneChanged);
        std::unique_ptr<QSignalSpy> aboutToKey;
        std::unique_ptr<QSignalSpy> flipped;
        if (MoxController* moxCtl = model->moxController()) {
            aboutToKey = std::make_unique<QSignalSpy>(moxCtl, &MoxController::txAboutToBegin);
            flipped = std::make_unique<QSignalSpy>(moxCtl, &MoxController::hardwareFlipped);
        }

        // Rx Ant 2 on the entry's slice (B) only; the grid lights from B.
        emit ant->antennaSelected(1);
        QCOMPARE(b->rxAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(a->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(ant->activeRxAntenna(), 1);

        // Tx Ant 2: the transmit slice (A), not the entry's slice.
        emit ant->antennaSelected(7);
        QCOMPARE(a->txAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(b->txAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(ant->activeTxAntenna(), 1);

        // Rx/Tx has nothing behind it: nothing changes.
        emit ant->antennaSelected(AntennaButtonItem::kRxTxButton);
        QCOMPARE(a->txAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(a->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(b->txAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(b->rxAntenna(), QStringLiteral("ANT2"));

        // The transmit slice moves to B: the Tx buttons show B's antenna,
        // and a Tx click now writes B only.
        TxSliceArbiter* arb = model->txSliceArbiter();
        QVERIFY(arb != nullptr);
        QVERIFY(arb->requestHandoff(b->sliceIndex()));
        QCOMPARE(model->txBoundSlice(), b);
        QCOMPARE(ant->activeTxAntenna(), 0);
        emit ant->antennaSelected(8);
        QCOMPARE(b->txAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(a->txAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(ant->activeTxAntenna(), 2);

        // An entry on another session's radio drives nothing here.
        const QString aRx = a->rxAntenna(), aTx = a->txAntenna();
        const QString bRx = b->rxAntenna(), bTx = b->txAntenna();
        ant->setProperty("containerSourceContext",
                         QJsonObject{{"sessionId", "another-radio"}, {"sliceId", a->sliceIndex()}});
        emit ant->antennaSelected(2);
        emit ant->antennaSelected(6);
        QCOMPARE(a->rxAntenna(), aRx);
        QCOMPARE(a->txAntenna(), aTx);
        QCOMPARE(b->rxAntenna(), bRx);
        QCOMPARE(b->txAntenna(), bTx);

        // Nothing keyed: no MOX, no TUNE, no key-down.
        QCOMPARE(mox.count(), 0);
        QCOMPARE(tune.count(), 0);
        QVERIFY(!model->transmitModel().isMox());
        if (aboutToKey) {
            QCOMPARE(aboutToKey->count(), 0);
            QCOMPARE(flipped->count(), 0);
            QVERIFY(!model->moxController()->isMox());
        }
        QVERIFY(sessions.replace({}, false));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstAntennaButtonItem test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_antenna_button_item.moc"
