// no-port-check: NereusSDR-original remote TX presentation regression.
//
// Verifies that the Phone/CW and VFO presentation gates suppress their
// TX-only writers in remote receive mode while preserving local/re-enabled
// interaction and authoritative model-to-widget updates.
//
// 2026-09-24: R-R3-49 (parity Task 1): the flag's filter-preset
// Shift-click hands the TX passband match to the window in a remote window
// too, whatever the keying gate says; MainWindow's transmit settings gate
// then applies it or says why. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
//
// 2026-09-24: R-R3-49 (parity Task 2): the Phone/CW applet's mic level,
// PROC, AM carrier and DEXP follow the transmit settings gate. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// 2026-09-25: R-R3-49, R-R3-21 (parity Task 11): the flag's XIT button,
// offset and zero write in a remote window whatever the transmit
// permission says. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
// Code.

#include <QtTest/QtTest>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFile>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTimer>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/widgets/ScrollableLabel.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
QPushButton* button(QWidget& parent, const QString& objectName)
{
    return parent.findChild<QPushButton*>(objectName);
}

QPushButton* buttonByText(QWidget& parent, const QString& text)
{
    for (QPushButton* candidate : parent.findChildren<QPushButton*>()) {
        if (candidate->text() == text) { return candidate; }
    }
    return nullptr;
}

struct TxContextMenuResult {
    bool found{false};
    bool enabled{false};
};

TxContextMenuResult invokeTxContextAction(VfoWidget& vfo)
{
    TxContextMenuResult result;
    QTimer::singleShot(0, &vfo, [&result, &vfo] {
        // Inspect the menu created by this flag, not the OS active popup:
        // offscreen/platform focus may still name the preceding menu.
        QMenu* menu = nullptr;
        for (QMenu* candidate : vfo.findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (candidate->isVisible()) { menu = candidate; break; }
        }
        if (!menu) { return; }
        for (QAction* action : menu->actions()) {
            if (action->text() == QStringLiteral("Make this the TX slice")) {
                result.found = true;
                result.enabled = action->isEnabled();
                action->trigger();
                break;
            }
        }
        menu->close();
    });

    const QPoint localPos(2, 2);
    QContextMenuEvent event(QContextMenuEvent::Mouse, localPos,
                            vfo.mapToGlobal(localPos));
    QApplication::sendEvent(&vfo, &event);
    return result;
}
}

class TestRemoteTxWidgets : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        const QString profile = QStringLiteral("remote-tx-widgets-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
    }

    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        AppSettings::instance().clear();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void remotePhoneDisablesWritersAndSetupHandoffs()
    {
        RadioModel remote(RadioModel::Role::Remote);
        PhoneCwApplet applet(&remote);

        QSlider* mic = nullptr;
        for (QSlider* candidate : applet.findChildren<QSlider*>()) {
            if (candidate->accessibleName() == QStringLiteral("Microphone gain")) {
                mic = candidate;
                break;
            }
        }
        QVERIFY(mic);
        auto* proc = button(applet, QStringLiteral("PhoneCwProcButton"));
        auto* procSlider = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        auto* vax = buttonByText(applet, QStringLiteral("VAX"));
        auto* dexp = button(applet, QStringLiteral("PhoneCwDexpButton"));
        QVERIFY(proc && procSlider && vax && dexp);
        QVERIFY(!mic->isEnabled());
        QVERIFY(!proc->isEnabled());
        QVERIFY(!procSlider->isEnabled());
        QVERIFY(!vax->isEnabled());
        QVERIFY(!dexp->isEnabled());

        QSignalSpy micSpy(&remote.transmitModel(), &TransmitModel::micGainDbChanged);
        QSignalSpy procSpy(&remote.transmitModel(), &TransmitModel::cpdrOnChanged);
        QSignalSpy procLevelSpy(&remote.transmitModel(), &TransmitModel::cpdrLevelDbChanged);
        QSignalSpy vaxSpy(&remote.transmitModel(), &TransmitModel::micSourceChanged);
        QSignalSpy dexpSpy(&remote.transmitModel(), &TransmitModel::dexpEnabledChanged);
        QSignalSpy setupSpy(&applet, &PhoneCwApplet::openSetupRequested);

        mic->setValue(mic->value() + 1);
        proc->click();
        procSlider->setValue(procSlider->value() + 1);
        vax->click();
        dexp->click();
        emit vax->customContextMenuRequested(QPoint());
        emit dexp->customContextMenuRequested(QPoint());

        QCOMPARE(micSpy.count(), 0);
        QCOMPARE(procSpy.count(), 0);
        QCOMPARE(procLevelSpy.count(), 0);
        QCOMPARE(vaxSpy.count(), 0);
        QCOMPARE(dexpSpy.count(), 0);
        // GUI-M3 (fix wave): the DEXP right-click opens Setup's DEXP/VOX
        // page, which gates its own controls; the VAX right-click stays
        // gated.
        QCOMPARE(setupSpy.count(), 1);
        QCOMPARE(setupSpy.first().at(1).toString(), QStringLiteral("DEXP/VOX"));

        // Remote snapshots still update the unavailable controls.
        remote.transmitModel().setCpdrOn(true);
        QCOMPARE(proc->isChecked(), true);

        // The gate must not erase the independent mic-mute rule while it is
        // active; changing that model value must leave the control gated.
        // R-R3-49 (parity Task 2): the mic level is a transmit setting, so
        // it follows setTransmitSettingsPermitted, not the keying gate.
        remote.transmitModel().setMicMute(false);
        QVERIFY(!mic->isEnabled());
        applet.setTransmitSettingsPermitted(true);
        QVERIFY(!mic->isEnabled());
        applet.setTransmitSettingsPermitted(false);
        remote.transmitModel().setMicMute(true);
        QVERIFY(!mic->isEnabled());
        applet.setTransmitPermitted(true);
        QVERIFY(!mic->isEnabled());
        applet.setTransmitSettingsPermitted(true);
        QVERIFY(mic->isEnabled());
    }

    // R-R3-49 (parity Task 2): the mic level, PROC and its level, AM carrier
    // and DEXP follow the transmit settings gate; the mic profile, mic
    // source and VAX keep the keying gate.
    void remotePhoneSettingsFollowTheTransmitSettingsGate()
    {
        RadioModel remote(RadioModel::Role::Remote);
        PhoneCwApplet applet(&remote);
        auto* proc = button(applet, QStringLiteral("PhoneCwProcButton"));
        auto* procSlider = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        auto* dexp = button(applet, QStringLiteral("PhoneCwDexpButton"));
        auto* vax = buttonByText(applet, QStringLiteral("VAX"));
        QVERIFY(proc && procSlider && dexp && vax);
        const QString coreReason = QStringLiteral(
            "This Core does not let this app change transmit settings. Updating the Core may help.");
        QCOMPARE(proc->toolTip(), coreReason);

        applet.setTransmitSettingsPermitted(true);
        QVERIFY(proc->isEnabled());
        QVERIFY(procSlider->isEnabled());
        QVERIFY(dexp->isEnabled());
        QVERIFY(!vax->isEnabled());
        const bool before = remote.transmitModel().cpdrOn();
        proc->click();
        QCOMPARE(remote.transmitModel().cpdrOn(), !before);

        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        applet.setTransmitSettingsPermitted(false, onAir);
        QVERIFY(!proc->isEnabled());
        QCOMPARE(proc->toolTip(), onAir);
    }

    void phoneGateRestoresLocalInteraction()
    {
        RadioModel local;
        PhoneCwApplet applet(&local);
        auto* proc = button(applet, QStringLiteral("PhoneCwProcButton"));
        QVERIFY(proc && proc->isEnabled());

        applet.setTransmitSettingsPermitted(false, QStringLiteral("remote reason"));
        QVERIFY(!proc->isEnabled());
        QCOMPARE(proc->toolTip(), QStringLiteral("remote reason"));
        applet.setTransmitSettingsPermitted(true);
        QVERIFY(proc->isEnabled());

        const bool before = local.transmitModel().cpdrOn();
        proc->click();
        QCOMPARE(local.transmitModel().cpdrOn(), !before);
    }

    void remoteVfoSuppressesTxOnlySignalsAndRestoresLocalInteraction()
    {
        RadioModel remote(RadioModel::Role::Remote);
        VfoWidget vfo;
        vfo.setSliceIndex(3);
        vfo.setRadioModel(&remote);
        vfo.show();
        QVERIFY(QTest::qWaitForWindowExposed(&vfo));

        auto* xit = button(vfo, QStringLiteral("VfoXitButton"));
        auto* zero = button(vfo, QStringLiteral("VfoXitZeroButton"));
        auto* badge = button(vfo, QStringLiteral("VfoTxBadge"));
        auto* bypass = button(vfo, QStringLiteral("m_rxBypassBtn"));
        QVERIFY(xit && zero && badge && bypass);
        auto* xitOffset = vfo.findChild<ScrollableLabel*>(QStringLiteral("VfoXitOffset"));
        // R-R3-49 (parity Task 11): XIT is a slice setting, live in a remote
        // window whatever the transmit permission says.
        QVERIFY(xit->isEnabled());
        QVERIFY(zero->isEnabled());
        QVERIFY(!badge->isEnabled());
        // Group B fix wave: BYPS waits for the Core to take RX bypass on TX
        // (MainWindow's setRxBypassPermitted), with a plain reason.
        QVERIFY(!bypass->isEnabled());
        QVERIFY(OperatorWording::isPlain(bypass->toolTip()));
        QVERIFY(xitOffset && xitOffset->isEnabled());

        QSignalSpy xitEnabledSpy(&vfo, &VfoWidget::xitEnabledChanged);
        QSignalSpy xitHzSpy(&vfo, &VfoWidget::xitHzChanged);
        QSignalSpy handoffSpy(&vfo, &VfoWidget::txHandoffRequested);
        QSignalSpy bypassSpy(&vfo, &VfoWidget::rxBypassToggled);
        xit->click();
        zero->click();
        xitOffset->setValue(100);
        badge->click();
        vfo.simulateTxBadgeClick();
        bypass->click();
        QCOMPARE(xitEnabledSpy.count(), 1);
        QVERIFY(xitEnabledSpy.first().first().toBool());
        QCOMPARE(xitHzSpy.count(), 2);
        QCOMPARE(xitHzSpy.at(0).first().toInt(), 0);
        QCOMPARE(xitHzSpy.at(1).first().toInt(), 100);
        QCOMPARE(handoffSpy.count(), 0);
        QCOMPARE(bypassSpy.count(), 0);

        const TxContextMenuResult remoteMenu = invokeTxContextAction(vfo);
        QVERIFY(remoteMenu.found);
        QVERIFY(!remoteMenu.enabled);
        QCOMPARE(handoffSpy.count(), 0);

        // The Core's value repaints the button without writing it back.
        vfo.setXitEnabled(false);
        QVERIFY(!xit->isChecked());
        QCOMPARE(xitEnabledSpy.count(), 1);

        vfo.setTransmitPermitted(true);
        QVERIFY(xit->isEnabled());
        QVERIFY(zero->isEnabled());
        QVERIFY(badge->isEnabled());
        QVERIFY(xitOffset->isEnabled());
        xit->click();
        vfo.simulateTxBadgeClick();
        QCOMPARE(xitEnabledSpy.count(), 2);
        QCOMPARE(handoffSpy.count(), 1);
        QCOMPARE(handoffSpy.first().first().toInt(), 3);

        const TxContextMenuResult localMenu = invokeTxContextAction(vfo);
        QVERIFY(localMenu.found);
        QVERIFY(localMenu.enabled);
        QCOMPARE(handoffSpy.count(), 2);
        QCOMPARE(handoffSpy.at(1).first().toInt(), 3);

        // Group B fix wave: BYPS follows its own gate, not the transmit
        // permission. With transmit withdrawn and the Core taking it, it
        // writes; with it refused, it waits with the reason.
        vfo.setTransmitPermitted(false);
        QVERIFY(!bypass->isEnabled());
        vfo.setRxBypassPermitted(true, QString());
        QVERIFY(bypass->isEnabled());
        bypass->click();
        QCOMPARE(bypassSpy.count(), 1);
        QVERIFY(bypassSpy.first().first().toBool());
        const QString older = QStringLiteral("This Core cannot switch its receive bypass on "
                                             "transmit for this app. Updating the Core may help.");
        QVERIFY(OperatorWording::isPlain(older));
        vfo.setRxBypassPermitted(false, older);
        QVERIFY(!bypass->isEnabled());
        QCOMPARE(bypass->toolTip(), older);
        vfo.setTransmitPermitted(true);
        QVERIFY(!bypass->isEnabled());
        bypass->click();
        QCOMPARE(bypassSpy.count(), 1);
    }

    // R-R3-49 (parity Task 1): the TX passband match is a transmit setting,
    // not a key. The remote flag still asks for it with the keying gate
    // closed; MainWindow decides with transmitSettingsPermitted().
    void remoteVfoShiftClickStillAsksForTheTxPassband()
    {
        RadioModel remote(RadioModel::Role::Remote);
        VfoWidget vfo;
        vfo.setSliceIndex(0);
        vfo.setRadioModel(&remote);
        vfo.setMode(DSPMode::USB);
        vfo.show();
        QVERIFY(QTest::qWaitForWindowExposed(&vfo));
        vfo.showTab(VfoWidget::Tab::Mode);

        QSignalSpy match(&vfo, &VfoWidget::txFilterMatchRequested);
        QPushButton* preset = nullptr;
        for (QPushButton* b : vfo.findChildren<QPushButton*>()) {
            if (b->isCheckable() && !b->isChecked() && b->isVisible()
                && b->toolTip().contains(QStringLiteral(" Hz to "))) {
                preset = b;
                break;
            }
        }
        QVERIFY(preset != nullptr);
        QTest::mouseClick(preset, Qt::LeftButton, Qt::ShiftModifier);
        QCOMPARE(match.count(), 1);
        const int low = match.first().at(0).toInt();
        const int high = match.first().at(1).toInt();
        QVERIFY(low >= 0);
        QVERIFY(high > low);
    }
};

QTEST_MAIN(TestRemoteTxWidgets)
#include "tst_remote_tx_widgets.moc"
