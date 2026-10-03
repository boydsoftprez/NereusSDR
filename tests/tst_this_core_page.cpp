// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_this_core_page.cpp  (NereusSDR)
// =================================================================
//
// Parity Task 21 (R-IOS-18, R-R3-38, R-R3-49; acceptance B6.2 and B6.3):
// the Core's radio from a real remote window.
//
//   - Radio > Change radio, and an unmanaged window's Manage Radios, open
//     Setup > This Core; before the session its controls are disabled with
//     "Connect to the Core to change these.".
//   - Connected, it lists the Core's radios (the Core's first and marked),
//     Use this radio reaches the Core (station.selectRadio), the model
//     choice sets the Core's override for that radio, and Forget is
//     disabled for the Core's own radio with the Core's reason.
//   - While the Core's radio is on the air every control is disabled with
//     the on-air reason.
//   - A Core that does not choose its radio: disabled with its reason.
//   - The station block and title bar menus offer Change radio, Edit radio
//     and Forget radio; the title bar copies the Core's radio's IP address
//     and MAC address.
//
// A local WebSocket and a static Core model; no radio, no audio device; the
// on-air case keys the Core model's own MoxController with its receive-only
// pre-check lifted.
//
//   cmake --build build --target tst_this_core_page
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_this_core_page$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include "core/AppSettings.h"
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/session/IStationLink.h"
#include "core/session/StationClient.h"
#include "core/station/StationRadios.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/TitleBar.h"
#include "gui/setup/ThisCorePage.h"
#include "gui/widgets/StationBlock.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const QString kG2Mac = QStringLiteral("AA:BB:CC:DD:EE:31");
const QString kHl2Mac = QStringLiteral("AA:BB:CC:DD:EE:32");

RadioInfo g2Radio()
{
    RadioInfo info;
    info.macAddress = kG2Mac;
    info.address = QHostAddress(QStringLiteral("192.168.1.31"));
    info.port = 1024;
    info.boardType = HPSDRHW::Saturn;
    info.protocol = ProtocolVersion::Protocol2;
    info.name = QStringLiteral("Bench G2");
    return info;
}

const QString kHermesMac = QStringLiteral("AA:BB:CC:DD:EE:33");

// A Hermes board (0x01), which can run as Red Pitaya (Thetis NetworkIO.cs
// board check, compatibleModels in HardwareProfile.cpp).
RadioInfo hermesRadio()
{
    RadioInfo info;
    info.macAddress = kHermesMac;
    info.address = QHostAddress(QStringLiteral("192.168.1.33"));
    info.boardType = HPSDRHW::Hermes;
    info.name = QStringLiteral("Bench Pitaya");
    return info;
}

RadioInfo hl2Radio()
{
    RadioInfo info;
    info.macAddress = kHl2Mac;
    info.address = QHostAddress(QStringLiteral("192.168.1.32"));
    info.boardType = HPSDRHW::HermesLite;
    info.name = QStringLiteral("Bench HL2");
    return info;
}

// What the popup `emitMenu` opens shows, read while it is up, then closed.
// `pick` (by text) is triggered first when given.
struct MenuEntry {
    QString text;
    bool enabled = false;
    QString toolTip;
};

QList<MenuEntry> popupEntries(const std::function<void()>& emitMenu, const QString& pick = {})
{
    QList<MenuEntry> seen;
    QTimer::singleShot(150, [&]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr) {
            return;
        }
        for (QAction* a : menu->actions()) {
            seen.append({a->text(), a->isEnabled(), a->toolTip()});
        }
        for (QAction* a : menu->actions()) {
            if (!pick.isEmpty() && a->text() == pick) {
                a->trigger();
                break;
            }
        }
        menu->close();
    });
    emitMenu();
    return seen;
}

ThisCorePage* openThisCore(RemoteWindowHarness& h, const QString& menuText)
{
    QAction* action = h.menuAction(QStringLiteral("&Radio"), menuText);
    if (action == nullptr) {
        return nullptr;
    }
    action->trigger();
    return h.window()->findChild<ThisCorePage*>();
}

void connectWindow(RemoteWindowHarness& h)
{
    QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
    QVERIFY(connect && connect->isEnabled());
    connect->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
}

} // namespace

class TstThisCorePage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("this-core-page")));
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    void theCoresRadioFromTheWindow()
    {
        RemoteWindowHarness h;
        h.station().setLastRadioInfoForTest(g2Radio());
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("core.settings")));
        StationRadios radios(coreSettings);
        QStringList selected;
        // As the Core's DaemonApp does (follow-up N3): an accepted change
        // holds its answer until the restart turn.
        radios.onSelect = [&](const QString& mac) {
            selected.append(mac);
            h.server().holdRadioChangeAnswers();
        };
        radios.setVisible({g2Radio(), hl2Radio()});
        radios.setCurrent(g2Radio());
        h.server().setStationRadios(&radios);
        QVERIFY(h.start());

        // B6.2: Radio > Change radio opens This Core; no session yet.
        QPointer<ThisCorePage> page = openThisCore(h, QStringLiteral("Change radio…"));
        QVERIFY(page);
        const QString connectWhy = QStringLiteral("Connect to the Core to change these.");
        QVERIFY(!page->scanButton()->isEnabled());
        QCOMPARE(page->scanButton()->toolTip(), connectWhy);
        QVERIFY(!page->useButton()->isEnabled());

        connectWindow(h);
        QCOMPARE(h.client()->capabilities().stationRadiosVersion, 1);
        // Setup rebuilds its pages when the Core's settings arrive.
        QTRY_VERIFY(!h.window()->findChildren<ThisCorePage*>().isEmpty()
                    && h.window()->findChildren<ThisCorePage*>().constLast()->radioList()
                               ->topLevelItemCount() == 2);
        page = h.window()->findChildren<ThisCorePage*>().constLast();
        QTRY_COMPARE(page->radioList()->topLevelItemCount(), 2);
        QTreeWidget* list = page->radioList();
        QCOMPARE(list->topLevelItem(0)->text(3), kG2Mac);
        QVERIFY(!list->topLevelItem(0)->text(4).isEmpty());
        QCOMPARE(list->topLevelItem(1)->text(3), kHl2Mac);

        // Fix wave, I5: this window signed in with the pairing token (a
        // bench link cannot enrol its key), so every control waits, with
        // the reason; and were one sent, the Core refuses it the same way.
        const QString paired = StationRadios::pairedDeviceReason();
        QTRY_COMPARE(page->scanButton()->toolTip(), paired);
        QVERIFY(!page->scanButton()->isEnabled());
        QVERIFY(!page->useButton()->isEnabled());
        QCOMPARE(page->statusLabel()->text(), paired);
        QCOMPARE(h.client()->requestStationRadio("station.rescanRadios", {}, 0).sent, true);
        QTRY_COMPARE(page->statusLabel()->text(), paired);
        // Follow-up N1: a token sign-in that enrolled this computer's key
        // (a desktop window's first session after an upgrade) is told to
        // reconnect, not to find a paired device: it is one.
        h.client()->setEnrolledDeviceKeyForTest(true);
        {
            QAction* drop = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Disconnect"));
            QVERIFY(drop && drop->isEnabled());
            drop->trigger();
        }
        QTRY_VERIFY(!h.client()->isHandshakeComplete());
        connectWindow(h);
        QTRY_VERIFY(!h.window()->findChildren<ThisCorePage*>().isEmpty()
                    && h.window()->findChildren<ThisCorePage*>().constLast()->radioList()
                               ->topLevelItemCount() == 2);
        page = h.window()->findChildren<ThisCorePage*>().constLast();
        const QString reconnect = ThisCorePage::reconnectToChangeRadioReason();
        QCOMPARE(reconnect, QStringLiteral("Reconnect this window to change the Core's radio."));
        QTRY_COMPARE(page->scanButton()->toolTip(), reconnect);
        QVERIFY(!page->scanButton()->isEnabled());
        QCOMPARE(page->statusLabel()->text(), reconnect);
        h.client()->setEnrolledDeviceKeyForTest(false);
        // From here the window and the Core act as a window signed in with
        // its own key (a desktop window after its enrolment).
        h.client()->setSignedInWithDeviceKeyForTest(true);
        h.server().setTokenSessionsMayChangeRadioForTest(true);
        QAction* disconnect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Disconnect"));
        QVERIFY(disconnect && disconnect->isEnabled());
        disconnect->trigger();
        QTRY_VERIFY(!h.client()->isHandshakeComplete());
        connectWindow(h);
        QTRY_VERIFY(!h.window()->findChildren<ThisCorePage*>().isEmpty()
                    && h.window()->findChildren<ThisCorePage*>().constLast()->radioList()
                               ->topLevelItemCount() == 2);
        page = h.window()->findChildren<ThisCorePage*>().constLast();
        list = page->radioList();
        list->setCurrentItem(list->topLevelItem(0));
        QTRY_VERIFY(page->scanButton()->isEnabled());

        // The Core's radio: nothing to use, and it cannot be forgotten.
        list->setCurrentItem(list->topLevelItem(0));
        QVERIFY(!page->useButton()->isEnabled());
        QVERIFY(!page->forgetButton()->isEnabled());
        QCOMPARE(page->forgetButton()->toolTip(), StationRadios::inUseReason());
        // Edit radio: the G2 runs as a G2 1K from its next connect.
        const int oneK = page->modelCombo()->findData(static_cast<int>(HPSDRModel::ANAN_G2_1K));
        QVERIFY(oneK >= 0);
        page->modelCombo()->setCurrentIndex(oneK);
        QTRY_COMPARE(coreSettings.modelOverride(kG2Mac), HPSDRModel::ANAN_G2_1K);

        // Change radio: the other radio, through the Core.
        list->setCurrentItem(list->topLevelItem(1));
        QVERIFY(page->useButton()->isEnabled());
        QVERIFY(page->forgetButton()->isEnabled());
        page->useButton()->click();
        QTRY_COMPARE(selected, QStringList{kHl2Mac});
        // Fix wave, I6: this run's choice until it connects.
        QCOMPARE(radios.pendingChoice(), kHl2Mac);
        QVERIFY(radios.savedChoice().isEmpty());
        // Follow-up N3: the answer waits for the restart turn. There the
        // radio was found on the air, so the change is dropped and this
        // window is told why, in the Core's on-air words.
        const QString changing =
            QStringLiteral("The Core is changing its radio. This window reconnects when it is ready.");
        QCOMPARE(page->statusLabel()->text(), changing);
        QTest::qWait(200);
        QCOMPARE(page->statusLabel()->text(), changing);
        radios.dropPendingChoice();
        h.server().finishRadioChange(false, RadioModel::onAirReason());
        QTRY_COMPARE(page->statusLabel()->text(), RadioModel::onAirReason());

        // B6.3: the title bar copies the Core's radio's address.
        QGuiApplication::clipboard()->clear();
        popupEntries([&]() { emit h.titleSegment()->contextMenuRequested(QPoint(5, 5)); },
                     QStringLiteral("Copy MAC address"));
        QTRY_COMPARE(QGuiApplication::clipboard()->text(), kG2Mac);
        popupEntries([&]() { emit h.titleSegment()->contextMenuRequested(QPoint(5, 5)); },
                     QStringLiteral("Copy IP address"));
        QTRY_COMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("192.168.1.31"));

        // B6.2: the station block and title bar menus offer the three.
        for (int which = 0; which < 2; ++which) {
            const QList<MenuEntry> entries = popupEntries([&]() {
                if (which == 0) {
                    emit h.stationBlock()->contextMenuRequested(QPoint(5, 5));
                } else {
                    emit h.titleSegment()->contextMenuRequested(QPoint(5, 5));
                }
            });
            QStringList texts;
            std::optional<MenuEntry> forget;
            for (const MenuEntry& e : entries) {
                texts.append(e.text);
                if (e.text == QStringLiteral("Forget radio")) {
                    forget = e;
                }
            }
            QVERIFY2(texts.contains(QStringLiteral("Change radio…")), qPrintable(texts.join('|')));
            QVERIFY2(texts.contains(QStringLiteral("Edit radio…")), qPrintable(texts.join('|')));
            QVERIFY(forget.has_value());
            // The Core's radio cannot be forgotten while it runs it.
            QVERIFY(!forget->enabled);
            QCOMPARE(forget->toolTip, StationRadios::inUseReason());
        }

        // On the air: every control waits, with the reason.
        radios.setSwitching(false);
        MoxController* const mox = h.station().moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QTRY_VERIFY(!page->scanButton()->isEnabled());
        QCOMPARE(page->scanButton()->toolTip(), RadioModel::onAirReason());
        QVERIFY(!page->useButton()->isEnabled());
        QCOMPARE(page->statusLabel()->text(), RadioModel::onAirReason());
        mox->setMox(false);
        QTRY_VERIFY(page->scanButton()->isEnabled());

        // Fix wave, M2: with no radio, the page shows the Core's own words
        // for why it waits.
        radios.clearCurrent();
        const QString waiting =
            QStringLiteral("The Core is waiting for its radio to appear on the network.");
        h.station().setStationRadioWaiting(waiting);
        QTRY_COMPARE(page->statusLabel()->text(), waiting);
        h.station().setStationRadioWaiting(QString());
        QTRY_COMPARE(page->statusLabel()->text(),
                     QStringLiteral("The Core is waiting for you to choose its radio."));
    }

    // Manage Radios offers the models the Core accepts for the radio's
    // board, from the Core's own list: a Hermes board run as Red Pitaya is
    // offered the Hermes board's models, never Orion MkII's (which the Core
    // refuses for it).
    void theModelChoiceIsTheCoresListForTheBoard()
    {
        RemoteWindowHarness h;
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("core.settings")));
        StationRadios radios(coreSettings);
        radios.setVisible({hermesRadio()});
        radios.setCurrent(hermesRadio());
        QVERIFY(radios.setModel(kHermesMac, static_cast<int>(HPSDRModel::REDPITAYA), nullptr));
        h.server().setStationRadios(&radios);
        h.server().setTokenSessionsMayChangeRadioForTest(true);
        QVERIFY(h.start());
        QVERIFY(openThisCore(h, QStringLiteral("Change radio…")) != nullptr);
        h.client()->setSignedInWithDeviceKeyForTest(true);
        connectWindow(h);
        QTRY_VERIFY(!h.window()->findChildren<ThisCorePage*>().isEmpty()
                    && h.window()->findChildren<ThisCorePage*>().constLast()->radioList()
                               ->topLevelItemCount() == 1);
        QPointer<ThisCorePage> page = h.window()->findChildren<ThisCorePage*>().constLast();
        page->radioList()->setCurrentItem(page->radioList()->topLevelItem(0));

        QComboBox* combo = page->modelCombo();
        QTRY_VERIFY(combo->isEnabled());
        QList<int> offered;
        for (int i = 0; i < combo->count(); ++i) {
            offered.append(combo->itemData(i).toInt());
        }
        QList<int> accepted;
        for (HPSDRModel m : compatibleModels(HPSDRHW::Hermes)) {
            accepted.append(static_cast<int>(m));
        }
        QCOMPARE(offered, accepted);
        QVERIFY(!offered.contains(static_cast<int>(HPSDRModel::ORIONMKII)));
        QCOMPARE(combo->currentData().toInt(), static_cast<int>(HPSDRModel::REDPITAYA));
        // Every choice is one the Core takes: back to Hermes.
        combo->setCurrentIndex(combo->findData(static_cast<int>(HPSDRModel::HERMES)));
        QTRY_COMPARE(coreSettings.modelOverride(kHermesMac), HPSDRModel::HERMES);
    }

    // A Core that does not send the model list: the choice shows the model
    // the Core runs the radio as, disabled, with the reason.
    void anOlderCoreLeavesTheModelChoiceDisabled()
    {
        RemoteWindowHarness h;
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("core.settings")));
        StationRadios radios(coreSettings);
        radios.setVisible({hermesRadio()});
        radios.setCurrent(hermesRadio());
        QVERIFY(radios.setModel(kHermesMac, static_cast<int>(HPSDRModel::REDPITAYA), nullptr));
        h.server().setStationRadios(&radios);
        h.server().setTokenSessionsMayChangeRadioForTest(true);
        QVERIFY(h.start());
        QVERIFY(openThisCore(h, QStringLiteral("Change radio…")) != nullptr);
        h.client()->setSignedInWithDeviceKeyForTest(true);
        h.client()->withholdFeatureForTest(QByteArrayLiteral("radioModels"));
        connectWindow(h);
        QTRY_VERIFY(!h.window()->findChildren<ThisCorePage*>().isEmpty()
                    && h.window()->findChildren<ThisCorePage*>().constLast()->radioList()
                               ->topLevelItemCount() == 1);
        QPointer<ThisCorePage> page = h.window()->findChildren<ThisCorePage*>().constLast();
        page->radioList()->setCurrentItem(page->radioList()->topLevelItem(0));
        QTRY_VERIFY(page->scanButton()->isEnabled());

        QComboBox* combo = page->modelCombo();
        QVERIFY(!combo->isEnabled());
        QCOMPARE(combo->toolTip(), ThisCorePage::modelListUnavailableReason());
        QCOMPARE(combo->count(), 1);
        QCOMPARE(combo->currentData().toInt(), static_cast<int>(HPSDRModel::REDPITAYA));
    }

    void aCoreThatDoesNotChooseItsRadioSaysSo()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        // An unmanaged remote window's Manage Radios opens This Core too.
        QVERIFY(openThisCore(h, QStringLiteral("&Manage Radios…")) != nullptr);
        connectWindow(h);
        QCOMPARE(h.client()->capabilities().stationRadiosVersion, 0);
        QTRY_VERIFY(!h.window()->findChildren<ThisCorePage*>().isEmpty());
        QPointer<ThisCorePage> page = h.window()->findChildren<ThisCorePage*>().constLast();
        QTRY_COMPARE(page->scanButton()->toolTip(),
                     IStationLink::stationRadiosUnavailableReason());
        QVERIFY(!page->scanButton()->isEnabled());
        QCOMPARE(page->statusLabel()->text(), IStationLink::stationRadiosUnavailableReason());
    }
};

QTEST_MAIN(TstThisCorePage)
#include "tst_this_core_page.moc"
