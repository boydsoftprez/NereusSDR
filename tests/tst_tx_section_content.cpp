// no-port-check: NereusSDR-original test file.
//
// =================================================================
// tests/tst_tx_section_content.cpp  (NereusSDR)
// =================================================================
//
// The TX applet's rows as independent container content.
//
// A container can hold the TX Monitor controls (or any other row) on their
// own.  The applet stays the wiring authority: it makes each copy and wires
// it, so the copy shows and sets the same model state as the applet's own
// row and follows the same transmit-permission gates.  The applet's own
// panel is unchanged — it still shows every row together.
//
// Tests:
//   1. The registry offers all seven row types.
//   2. A copy is its own instance, not the applet's own row, and two copies
//      are two instances.
//   3. The copy shows the model state.
//   4. The copy's control sets the model, and the applet's own row follows.
//   5. The applet's own panel still has every row.
//   6. The transmit-permission gate reaches the copy.
//  10. A copy made after the gates carries them, with their reasons.
//  11. A copy made while receive only is locked like the applet's row.
//  12. A remote window's 2-TONE copies follow the Core.
//  13. A copy's Take transmit asks as the applet's own does.
//  14. A preview of a row is inert.
//  15. An unkey clears every power row's reading.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-21 - Copies made after the gates, remote 2-TONE copies, a copy's
//                Take transmit and inert previews; the unreachable "+" menu
//                test goes. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-10-20 - The tests for the TX rows as container content, with the
//                crash on closing a container that held a copy, and a
//                copy's margins. Brian Sutton, AI-assisted via OpenCode.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMargins>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTimer>
#include <QWidget>

#include <type_traits>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/MoxController.h"
#include "core/RadioStatus.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxChainSection.h"
#include "gui/applets/TxFilterSection.h"
#include "gui/applets/TxKeyingSection.h"
#include "gui/applets/TxMonitorSection.h"
#include "gui/applets/TxPowerSection.h"
#include "gui/applets/TxProcessingSection.h"
#include "gui/applets/TxVoxSection.h"
#include "gui/applets/TxSectionContent.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

const char* const kRowTypes[] = {
    "tx.monitor", "tx.power", "tx.filter", "tx.chain",
    "tx.vox", "tx.processing", "tx.keying",
};

// The applet's own row's widgets, by the object names the stylesheet and the
// rest of the app find them by.
const char* const kAppletRowWidgets[] = {
    "TxLevButton", "TxEqButton", "TxCfcButton", "TxRfPowerSlider",
    "TxVoxButton", "TxAppletPsaBtn", "TxHolderLabel", "TxTakeTransmitButton",
};

} // namespace

class TestTxSectionContent : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. The registry offers every row type ────────────────────────────
    void registryOffersEveryRow()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        for (const char* type : kRowTypes) {
            const QString id = QString::fromLatin1(type);
            QVERIFY2(registry.hasWidgetFactory(id), qPrintable(id));
            QVERIFY2(registry.isAvailable(id), qPrintable(id));

            bool listed = false;
            for (const auto& descriptor : registry.descriptors()) {
                if (descriptor.typeId == id) {
                    listed = true;
                    QVERIFY2(!descriptor.title.isEmpty(), qPrintable(id));
                    break;
                }
            }
            QVERIFY2(listed, qPrintable(id));
        }
    }

    // ── 2. A copy is its own instance ───────────────────────────────────
    void aCopyIsItsOwnInstance()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        QVERIFY(copy != &applet);

        auto* copyLev = copy->findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        auto* ownLev = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(copyLev);
        QVERIFY(ownLev);
        QVERIFY(copyLev != ownLev);

        // A second copy is a second instance, not the first one again.
        auto* second = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(second);
        QVERIFY(second != copy);
        auto* secondLev = second->findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(secondLev);
        QVERIFY(secondLev != copyLev);
        QVERIFY(secondLev != ownLev);
    }

    // ── 3. The copy shows the model state ────────────────────────────────
    void theCopyShowsTheModelState()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        auto* chain = qobject_cast<TxChainSection*>(copy);
        QVERIFY(chain);
        QVERIFY(chain->levButton());

        // The model starts with the leveler on.
        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
        QVERIFY(chain->levButton()->isChecked());

        rm.transmitModel().setTxLevelerOn(false);
        QVERIFY(!chain->levButton()->isChecked());

        rm.transmitModel().setTxLevelerOn(true);
        QVERIFY(chain->levButton()->isChecked());
    }

    // ── 4. The copy sets the model, and the applet's own row follows ─────
    void theCopySetsTheModelAndTheAppletFollows()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        auto* chain = qobject_cast<TxChainSection*>(copy);
        QVERIFY(chain);
        QVERIFY(chain->levButton());
        auto* ownLev = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(ownLev);

        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
        QVERIFY(ownLev->isChecked());

        // The copy's press is the model's answer.
        chain->levButton()->click();
        QCOMPARE(rm.transmitModel().txLevelerOn(), false);
        QVERIFY(!ownLev->isChecked());

        // And back the other way: the applet's own press moves the copy.
        ownLev->click();
        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
        QVERIFY(chain->levButton()->isChecked());
    }

    // ── 5. The applet's own panel still has every row ────────────────────
    void theAppletKeepsEveryRow()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        for (const char* name : kAppletRowWidgets) {
            const QString objectName = QString::fromLatin1(name);
            QVERIFY2(applet.findChild<QWidget*>(objectName), qPrintable(objectName));
        }
    }

    // ── 6. The registry is the authority on what the workspace can build ─
    void theRegistryIsTheAuthorityOnTheRows()
    {
        // A workspace with no TX rows registered: the rows are not offered.
        ContainerContentRegistry bare;
        for (const char* type : kRowTypes) {
            QVERIFY2(!bare.hasWidgetFactory(QString::fromLatin1(type)), type);
        }

        // A workspace the app registered them in: every row is offered.
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);
        for (const char* type : kRowTypes) {
            QVERIFY2(registry.hasWidgetFactory(QString::fromLatin1(type)), type);
        }
    }

    // ── 8b. The operator adds a row from the item list ──────────────────
    // In the workspace, Container Settings hides the "+" popup and offers
    // the registry's content in its item list instead.  Pick a TX row there,
    // press "Add ->" and Apply, as the operator does, and the container
    // holds a working copy of the row.
    void theOperatorAddsARowFromTheItemList()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings"));
        ContainerWorkspaceStore store(settings);
        MeterPoller poller;
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        manager.setPreviewPoller(&poller);
        WorkspaceDocument document;
        document.mainContainerId = QStringLiteral("A");
        ContainerDocument container;
        container.id = QStringLiteral("A");
        container.layout = ContentLayout::VerticalStack;
        document.containers = {container};
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);

        ContainerSettingsDialog dialog(manager.container("A"), nullptr, &manager);
        dialog.show();
        QCoreApplication::processEvents();

        auto* available = dialog.findChild<QListWidget*>(QStringLiteral("containerAvailableContents"));
        QVERIFY(available);
        QStringList offered;
        QListWidgetItem* keying = nullptr;
        for (int i = 0; i < available->count(); ++i) {
            QListWidgetItem* item = available->item(i);
            if (item->data(Qt::UserRole).toString().startsWith(QStringLiteral("tx."))) {
                offered << item->text();
                QVERIFY(item->flags() & Qt::ItemIsEnabled);
            }
            if (item->data(Qt::UserRole).toString() == QStringLiteral("tx.keying")) {
                keying = item;
            }
        }
        QCOMPARE(offered.size(), 7);
        QVERIFY(keying);
        available->setCurrentItem(keying);

        QPushButton* addButton = nullptr;
        QPushButton* applyButton = nullptr;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->toolTip() == QStringLiteral("Add the selected available item to the in-use list")) {
                addButton = button;
            }
            if (button->text() == QStringLiteral("Apply")) {
                applyButton = button;
            }
        }
        QVERIFY(addButton && addButton->isVisible());
        QVERIFY(applyButton);
        addButton->click();
        QCoreApplication::processEvents();
        applyButton->click();
        QCoreApplication::processEvents();

        auto* copy = manager.container("A")->findChild<TxKeyingSection*>();
        QVERIFY(copy);
        QVERIFY(copy->tuneButton());
        QVERIFY(copy->moxButton());
    }

    // ── 9. A copy gives itself the margins the applet's column gives ─────
    // The applet's own rows give none, because their column already has
    // the margins.  A copy in a container has no column of its own, so it
    // gives itself the margins the column would have given it.
    void aCopyGivesItselfTheColumnMargins()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        auto* chain = qobject_cast<TxChainSection*>(copy);
        QVERIFY(chain);

        const QMargins copyMargins = chain->layout()->contentsMargins();
        QCOMPARE(copyMargins.left(), 4);
        QCOMPARE(copyMargins.right(), 4);
        QCOMPARE(copyMargins.top(), 2);
        QCOMPARE(copyMargins.bottom(), 2);

        // The applet's own row gives none: its column already has them.
        auto* own = applet.findChild<TxChainSection*>();
        QVERIFY(own);
        const QMargins ownMargins = own->layout()->contentsMargins();
        QCOMPARE(ownMargins.left(), 0);
        QCOMPARE(ownMargins.right(), 0);
        QCOMPARE(ownMargins.top(), 0);
        QCOMPARE(ownMargins.bottom(), 0);
    }

    // ── 8. A copy the container destroys leaves the applet's list ─────────
    // The copy belongs to the container, which owns it: closing the
    // container destroys the copy.  The applet's timers and gates walk its
    // list of copies, so a copy that is gone must not stay in it.
    void aCopyTheContainerDestroysLeavesTheAppletsList()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        QList<QWidget*> copies;
        for (const char* type : kRowTypes) {
            auto* copy = registry.createWidget(
                registry.makeEntry(QString::fromLatin1(type)), &host);
            QVERIFY(copy);
            copies << copy;
        }

        // While the copies live, the applet's pushers and gates reach them.
        applet.syncFromModel();
        applet.setTransmitPermitted(true);

        // The container closes: every copy goes with it.
        for (QWidget* copy : copies) {
            delete copy;
        }

        // The applet's pushers and gates must not touch a copy that is gone.
        applet.syncFromModel();
        applet.setTransmitPermitted(false);
        applet.setTransmitSettingsPermitted(false);
        applet.setTransmitChainSettingsPermitted(false);
        applet.setVoxPermitted(false);
        applet.setTxProfilePermitted(false);
        applet.setPureSignalArmingPermitted(false);
        applet.setTransmitPermitted(true);
        applet.setTransmitSettingsPermitted(true);
        applet.setTransmitChainSettingsPermitted(true);
        applet.setVoxPermitted(true);
        applet.setTxProfilePermitted(true);
        applet.setPureSignalArmingPermitted(true);

        // The applet's own row still works.
        QVERIFY(applet.findChild<QPushButton*>(QStringLiteral("TxLevButton")));
    }

    // ── 9. The model's signals do not reach a copy that is gone ──────────
    // A copy's model wiring belongs to the copy.  After the container
    // destroys it, a model change must not write to its widgets; the copy's
    // own refresh timer must stop with it too.  Run with MallocScribble=1 on
    // macOS (or under ASan) so a write to freed memory fails every time.
    void theModelDoesNotReachACopyThatIsGone()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        const int timersBefore = applet.findChildren<QTimer*>().size();
        QWidget host;
        QList<QWidget*> copies;
        for (const char* type : kRowTypes) {
            auto* copy = registry.createWidget(
                registry.makeEntry(QString::fromLatin1(type)), &host);
            QVERIFY(copy);
            copies << copy;
        }
        for (QWidget* copy : copies) {
            delete copy;
        }
        // Every model signal the power, filter, chain and VOX rows follow.
        TransmitModel& tx = rm.transmitModel();
        tx.setPower(tx.power() == 37 ? 38 : 37);
        tx.setTunePowerForBand(Band::Band20m, 23);
        tx.setFilterLow(tx.filterLow() + 10);
        tx.setFilterHigh(tx.filterHigh() - 10);
        tx.setTxLevelerOn(!tx.txLevelerOn());
        tx.setTxEqEnabled(!tx.txEqEnabled());
        tx.setCfcEnabled(!tx.cfcEnabled());
        tx.setVoxEnabled(!tx.voxEnabled());
        tx.setVoxThresholdDb(tx.voxThresholdDb() == -40 ? -41 : -40);
        tx.setVoxHangTimeMs(tx.voxHangTimeMs() == 300 ? 310 : 300);
        rm.radioStatus().setForwardPower(12.5);
        QTest::qWait(120);   // the 20 Hz power refresh and the 100 ms VOX poll

        // The applet's own row still follows the model.
        auto* own = applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
        QVERIFY(own);
        QCOMPARE(own->value(), tx.power());

        // No timer a copy started outlives it.
        QCOMPARE(applet.findChildren<QTimer*>().size(), timersBefore);
    }

    // ── 6. The transmit-permission gate reaches the copy ─────────────────
    void theTransmitGateReachesTheCopy()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.keying"), &host);
        QVERIFY(copy);
        auto* keying = qobject_cast<TxKeyingSection*>(copy);
        QVERIFY(keying);
        QVERIFY(keying->moxButton());
        QVERIFY(keying->tuneButton());
        QVERIFY(keying->moxButton()->isEnabled());
        QVERIFY(keying->tuneButton()->isEnabled());

        const QString reason = QStringLiteral("The Core will not transmit");
        applet.setTransmitPermitted(false, reason);

        QVERIFY(!keying->moxButton()->isEnabled());
        QVERIFY(!keying->tuneButton()->isEnabled());
        QCOMPARE(keying->moxButton()->toolTip(), reason);

        // The applet's own row is gated the same way.
        QVERIFY(!applet.moxButton()->isEnabled());
        QVERIFY(!applet.tuneButton()->isEnabled());

        applet.setTransmitPermitted(true);
        QVERIFY(keying->moxButton()->isEnabled());
        QVERIFY(keying->tuneButton()->isEnabled());
    }

    // ── 10. A copy made after the gates carries them ─────────────────────
    // The gates are applied when the window learns what the Core allows,
    // usually before the operator places a row in a container.  A copy
    // made later must start exactly as gated as the applet's own row, with
    // the same reasons, or its MOX and TUNE would look live.
    void aCopyMadeAfterTheGatesCarriesThem_data()
    {
        QTest::addColumn<bool>("remote");
        QTest::addColumn<bool>("chainPermitted");
        QTest::newRow("local, every gate") << false << false;
        QTest::newRow("remote, every gate") << true << false;
        QTest::newRow("local, monitor output only") << false << true;
        QTest::newRow("remote, monitor output only") << true << true;
    }
    void aCopyMadeAfterTheGatesCarriesThem()
    {
        QFETCH(bool, remote);
        QFETCH(bool, chainPermitted);
        RadioModel rm(remote ? RadioModel::Role::Remote : RadioModel::Role::Local);
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);
        // TUNE on the Tune slider, so only the chain gate decides that
        // slider here (the drive source gate grays it under Drive Slider).
        rm.transmitModel().setTuneDrivePowerSource(DrivePowerSource::TuneSlider);

        const QString txReason = QStringLiteral("Transmit is not yours");
        applet.setTransmitPermitted(false, txReason);
        applet.setVoxPermitted(false, QStringLiteral("No microphone line"));
        applet.setTransmitSettingsPermitted(false, QStringLiteral("Settings are the Core's"));
        applet.setTransmitChainSettingsPermitted(chainPermitted,
                                                 QStringLiteral("Chain is the Core's"));
        applet.setTxProfilePermitted(false, QStringLiteral("Profiles are the Core's"));
        applet.setMonitorOutputPermitted(false, QStringLiteral("The Core sends no MON"));
        applet.setPureSignalArmingPermitted(false, QStringLiteral("PS-A is the Core's"));
        BoardCapabilities caps{};
        caps.board = HPSDRHW::Atlas;
        caps.hasPureSignal = false;
        applet.setBoardCapabilities(caps);

        QWidget host;
        QList<QWidget*> copies;
        for (const char* type : kRowTypes) {
            QWidget* copy = registry.createWidget(
                registry.makeEntry(QString::fromLatin1(type)), &host);
            QVERIFY2(copy, type);
            copies << copy;
        }
        const auto copyOf = [&copies](auto* own) {
            using Section = std::remove_pointer_t<decltype(own)>;
            for (QWidget* copy : copies) {
                if (auto* s = qobject_cast<Section*>(copy)) {
                    return s;
                }
            }
            return static_cast<Section*>(nullptr);
        };

        auto* ownKeying = applet.findChild<TxKeyingSection*>();
        auto* ownVox = applet.findChild<TxVoxSection*>();
        auto* ownPower = applet.findChild<TxPowerSection*>();
        auto* ownFilter = applet.findChild<TxFilterSection*>();
        auto* ownChain = applet.findChild<TxChainSection*>();
        auto* ownMonitor = applet.findChild<TxMonitorSection*>();
        auto* ownProcessing = applet.findChild<TxProcessingSection*>();
        QVERIFY(ownKeying && ownVox && ownPower && ownFilter && ownChain && ownMonitor
                && ownProcessing);
        auto* keying = copyOf(ownKeying);
        auto* vox = copyOf(ownVox);
        auto* power = copyOf(ownPower);
        auto* filter = copyOf(ownFilter);
        auto* chain = copyOf(ownChain);
        auto* monitor = copyOf(ownMonitor);
        auto* processing = copyOf(ownProcessing);
        QVERIFY(keying && vox && power && filter && chain && monitor && processing);

        const QList<QPair<QWidget*, QWidget*>> pairs = {
            {ownKeying->moxButton(), keying->moxButton()},
            {ownKeying->tuneButton(), keying->tuneButton()},
            {ownVox->button(), vox->button()},
            {ownVox->thresholdSlider(), vox->thresholdSlider()},
            {ownVox->holdSlider(), vox->holdSlider()},
            {ownPower->rfPowerSlider(), power->rfPowerSlider()},
            {ownPower->tunePowerSlider(), power->tunePowerSlider()},
            {ownFilter->filterLowSpin(), filter->filterLowSpin()},
            {ownFilter->filterHighSpin(), filter->filterHighSpin()},
            {ownChain->levButton(), chain->levButton()},
            {ownChain->eqButton(), chain->eqButton()},
            {ownChain->cfcButton(), chain->cfcButton()},
            {ownChain->profileCombo(), chain->profileCombo()},
            {ownMonitor->monButton(), monitor->monButton()},
            {ownMonitor->volumeSlider(), monitor->volumeSlider()},
            {ownMonitor->speakersButton(), monitor->speakersButton()},
            {ownMonitor->headphonesButton(), monitor->headphonesButton()},
            {ownProcessing->twoToneButton(), processing->twoToneButton()},
            {ownProcessing->psaButton(), processing->psaButton()},
        };
        for (const auto& pair : pairs) {
            QVERIFY(pair.first && pair.second);
            const QString name = pair.first->objectName();
            QVERIFY2(pair.first->isEnabled() == pair.second->isEnabled(), qPrintable(name));
            QVERIFY2(pair.first->toolTip() == pair.second->toolTip(),
                     qPrintable(name + QStringLiteral(": ") + pair.first->toolTip()
                                + QStringLiteral(" / ") + pair.second->toolTip()));
            QVERIFY2(pair.first->isHidden() == pair.second->isHidden(), qPrintable(name));
        }

        // The keys themselves: disabled, with the transmit gate's reason.
        for (QWidget* key : {static_cast<QWidget*>(keying->moxButton()),
                             static_cast<QWidget*>(keying->tuneButton()),
                             static_cast<QWidget*>(vox->button())}) {
            QVERIFY(!key->isEnabled());
            QCOMPARE(key->toolTip(), txReason);
        }
        QVERIFY(!power->rfPowerSlider()->isEnabled());
        QVERIFY(!monitor->speakersButton()->isEnabled());
        QCOMPARE(monitor->speakersButton()->toolTip(),
                 chainPermitted ? QStringLiteral("The Core sends no MON")
                                : QStringLiteral("Chain is the Core's"));
        QCOMPARE(power->tunePowerSlider()->isEnabled(), chainPermitted);
        QVERIFY(processing->psaButton()->isHidden());
    }

    // ── 11. A copy made while receive only is locked like the applet's ───
    void aCopyMadeWhileReceiveOnlyIsLocked()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        rm.setRxOnly(true);
        QVERIFY(rm.transmitButtonsLocked());
        QVERIFY(!applet.tuneButton()->isEnabled());

        QWidget host;
        auto* keying = qobject_cast<TxKeyingSection*>(
            registry.createWidget(registry.makeEntry("tx.keying"), &host));
        auto* processing = qobject_cast<TxProcessingSection*>(
            registry.createWidget(registry.makeEntry("tx.processing"), &host));
        auto* vox = qobject_cast<TxVoxSection*>(
            registry.createWidget(registry.makeEntry("tx.vox"), &host));
        QVERIFY(keying && processing && vox);

        const QList<QPair<QWidget*, QWidget*>> pairs = {
            {applet.moxButton(), keying->moxButton()},
            {applet.tuneButton(), keying->tuneButton()},
            {applet.twoToneButton(), processing->twoToneButton()},
            {applet.voxButton(), vox->button()},
        };
        for (const auto& pair : pairs) {
            const QString name = pair.first->objectName();
            QVERIFY2(pair.first->isEnabled() == pair.second->isEnabled(), qPrintable(name));
            QVERIFY2(pair.first->toolTip() == pair.second->toolTip(), qPrintable(name));
        }
        QVERIFY(!keying->tuneButton()->isEnabled());
        QCOMPARE(keying->tuneButton()->toolTip(), rm.rxOnlyReason());

        // And the lock comes off the copy with the applet's.
        rm.setRxOnly(false);
        QVERIFY(keying->tuneButton()->isEnabled());
        QVERIFY(keying->moxButton()->isEnabled());
    }

    // ── 12. A remote window's 2-TONE copies follow the Core ──────────────
    void aRemoteTwoToneCopyFollowsTheCore()
    {
        RadioModel rm(RadioModel::Role::Remote);
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* processing = qobject_cast<TxProcessingSection*>(
            registry.createWidget(registry.makeEntry("tx.processing"), &host));
        QVERIFY(processing && processing->twoToneButton());
        QVERIFY(!processing->twoToneButton()->isChecked());

        QVERIFY(rm.pureSignalFacade()->applyRemoteProperty("twoToneOn", true));
        QVERIFY(applet.twoToneButton()->isChecked());
        QVERIFY(processing->twoToneButton()->isChecked());

        QVERIFY(rm.pureSignalFacade()->applyRemoteProperty("twoToneOn", false));
        QVERIFY(!processing->twoToneButton()->isChecked());

        // A press the Core refuses is unchecked again.
        {
            const QSignalBlocker block(processing->twoToneButton());
            processing->twoToneButton()->setChecked(true);
        }
        rm.reportRemoteTransmitRefused(QStringLiteral("test refusal"));
        QVERIFY(!processing->twoToneButton()->isChecked());
    }

    // ── 13. A copy's Take transmit asks as the applet's own does ─────────
    void aCopysTakeTransmitAsks()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* keying = qobject_cast<TxKeyingSection*>(
            registry.createWidget(registry.makeEntry("tx.keying"), &host));
        QVERIFY(keying && keying->takeTransmitButton());
        applet.setTakeTransmitOffered(true, false);
        QVERIFY(!keying->takeTransmitButton()->isHidden());

        QSignalSpy asked(&applet, &TxApplet::takeTransmitRequested);
        keying->takeTransmitButton()->click();
        QCOMPARE(asked.count(), 1);
        applet.takeTransmitButton()->click();
        QCOMPARE(asked.count(), 2);
    }

    // ── 14. A preview of a row is inert ──────────────────────────────────
    // Container Settings builds previews of the rows it shows.  A preview
    // must not key, take focus, follow the model or join the applet's rows.
    void aPreviewOfARowIsInert()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QSignalSpy moxChanges(rm.moxController(), &MoxController::moxStateChanged);
        QSignalSpy manualChanges(rm.moxController(), &MoxController::manualMoxChanged);
        QSignalSpy asked(&applet, &TxApplet::takeTransmitRequested);

        QWidget host;
        auto* keying = qobject_cast<TxKeyingSection*>(
            registry.createPreview(registry.makeEntry("tx.keying"), &host));
        QVERIFY(keying && keying->moxButton() && keying->tuneButton());
        for (QWidget* child : keying->findChildren<QWidget*>()) {
            QVERIFY2(child->focusPolicy() == Qt::NoFocus, qPrintable(child->objectName()));
            QVERIFY2(!child->isEnabled(), qPrintable(child->objectName()));
        }

        // Neither a click nor the keyboard toggles MOX or TUNE.
        for (QPushButton* key : {keying->moxButton(), keying->tuneButton(),
                                 keying->takeTransmitButton()}) {
            key->click();
            QVERIFY(!key->isChecked());
            QTest::keyClick(key, Qt::Key_Space);
            QVERIFY(!key->isChecked());
        }
        QCOMPARE(moxChanges.count(), 0);
        QCOMPARE(manualChanges.count(), 0);
        QCOMPARE(asked.count(), 0);
        QVERIFY(!rm.moxController()->isMox());

        // The preview is not one of the applet's rows: the gates and the
        // model pushes pass it by.
        const QString tip = keying->moxButton()->toolTip();
        applet.setTransmitPermitted(false, QStringLiteral("Gated"));
        QCOMPARE(keying->moxButton()->toolTip(), tip);
        applet.setTransmitPermitted(true);
        {
            const QSignalBlocker block(keying->moxButton());
            keying->moxButton()->setChecked(true);
        }
        applet.syncFromModel();
        QVERIFY(keying->moxButton()->isChecked());

        // A model change does not reach a preview.
        auto* chain = qobject_cast<TxChainSection*>(
            registry.createPreview(registry.makeEntry("tx.chain"), &host));
        QVERIFY(chain && chain->levButton());
        const bool shown = chain->levButton()->isChecked();
        rm.transmitModel().setTxLevelerOn(!rm.transmitModel().txLevelerOn());
        QCOMPARE(chain->levButton()->isChecked(), shown);
    }

    // ── 15. An unkey clears every power row's reading ────────────────────
    // One handler per model signal serves every power row; each row's
    // smoothed forward power still drops to zero on an unkey.
    void anUnkeyClearsEveryPowerRow()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* first = qobject_cast<TxPowerSection*>(
            registry.createWidget(registry.makeEntry("tx.power"), &host));
        auto* second = qobject_cast<TxPowerSection*>(
            registry.createWidget(registry.makeEntry("tx.power"), &host));
        auto* own = applet.findChild<TxPowerSection*>();
        QVERIFY(first && second && own);

        rm.radioStatus().setForwardPower(50.0);
        for (TxPowerSection* row : {own, first, second}) {
            QVERIFY(row->smoothedForwardPowerW() > 0.0);
        }
        emit rm.moxController()->moxStateChanged(false);
        for (TxPowerSection* row : {own, first, second}) {
            QCOMPARE(row->smoothedForwardPowerW(), 0.0);
        }

        // And again from the mirrored transmit state a remote window uses.
        rm.radioStatus().setForwardPower(50.0);
        QVERIFY(second->smoothedForwardPowerW() > 0.0);
        emit rm.transmittingChanged(false);
        for (TxPowerSection* row : {own, first, second}) {
            QCOMPARE(row->smoothedForwardPowerW(), 0.0);
        }
    }

    void keyingCanHideTheSliceIndicators()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        const QVector<ContentProperty> properties = registry.declaredProperties(QStringLiteral("tx.keying"));
        QCOMPARE(properties.size(), 1);
        QCOMPARE(properties.at(0).key, QStringLiteral("hideSliceIndicators"));
        QCOMPARE(properties.at(0).label, QStringLiteral("Hide slice indicators"));

        QWidget host;
        auto* shown = qobject_cast<TxKeyingSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("tx.keying")), &host));
        QVERIFY(shown);
        QVERIFY(shown->tuneButton());
        QVERIFY(shown->moxButton());
        QVERIFY(!shown->tuneButton()->isHidden());
        QVERIFY(!shown->moxButton()->isHidden());
        QVERIFY(shown->sliceIndicatorRow());
        QVERIFY(!shown->sliceIndicatorRow()->isHidden());

        ContentEntry hidden = registry.makeEntry(QStringLiteral("tx.keying"));
        hidden.config.insert(QStringLiteral("hideSliceIndicators"), true);
        auto* folded = qobject_cast<TxKeyingSection*>(registry.createWidget(hidden, &host));
        QVERIFY(folded);
        QVERIFY(folded->sliceIndicatorsHidden());
        QVERIFY(folded->sliceIndicatorRow()->isHidden());
        QVERIFY(!folded->tuneButton()->isHidden());
        QVERIFY(!folded->moxButton()->isHidden());
        QVERIFY(folded->holderLabel()->isHidden());
        QVERIFY(folded->takeTransmitButton()->isHidden());

        applet.setTransmitHolderText(QStringLiteral("Another device holds transmit"));
        applet.setTakeTransmitOffered(true, false);
        QVERIFY(folded->holderLabel()->isHidden());
        QVERIFY(folded->takeTransmitButton()->isHidden());
        QVERIFY(!shown->holderLabel()->isHidden());
        QVERIFY(!shown->takeTransmitButton()->isHidden());
    }
};

QTEST_MAIN(TestTxSectionContent)
#include "tst_tx_section_content.moc"
