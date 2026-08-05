// =================================================================
// tests/tst_tnf_ui_wiring.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Nothing here is
// a port; the surfaces under test are fixed by
// docs/architecture/2026-07-28-tunable-notch-filter-design.md sections
// 7.5, 9 (overlay stub retirement) and 10.2 (shortcut scope).
//
// TNF build-order step 8: the operator-facing TNF controls that live
// outside the panadapter itself.
//
//   1. The +TNF button on SpectrumOverlayPanel is live and carries the id
//      of the pan it is drawn on. The disabled "MNF" twin beside it is
//      gone: design section 9, "Replace it rather than shipping both."
//   2. NotchModel::tnfAddCenterHz composes the centre that button asks
//      for, VFO + RIT shifted into the sideband (section 7.5).
//   3. The status-bar TNF light's stylesheet and tooltip are pure
//      functions of (global enable, notch count), and the DSP-menu
//      accelerator is a named constant (section 10.2: no
//      shortcut-assignment subsystem exists, so the chord is fixed rather
//      than registered).
//   4. Menu action and status-bar light both follow
//      NotchModel::globalEnabledChanged, and a flip from either surface
//      reaches the model exactly once.
//   5. NotchModel::notchAddRejected reaches operator-visible feedback.
//
// MainWindow needs a full RadioModel (WDSP, audio, network) to construct,
// which no unit-test executable can afford; see the header of
// tst_mainwindow_status_bar_safety.cpp. The MainWindow surfaces under test
// are therefore either public statics, callable without an instance, or
// resolved by name through staticMetaObject the way
// tst_notch_hit_test.cpp resolves the notch fan-out slots.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QKeySequence>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/StyleConstants.h"
#include "models/NotchModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestTnfUiWiring : public QObject {
    Q_OBJECT

private:
    // SpectrumOverlayPanel parents its flyouts to parentWidget() (the host
    // SpectrumWidget in production, a bare QWidget here). Same harness as
    // tst_spectrum_overlay_panel.cpp.
    struct PanelHarness {
        QWidget host;
        SpectrumOverlayPanel* panel{nullptr};
        PanelHarness() {
            panel = new SpectrumOverlayPanel(&host);
        }
    };

    static QPushButton* buttonWithText(QWidget& host, const QString& text) {
        const QList<QPushButton*> btns = host.findChildren<QPushButton*>();
        for (QPushButton* b : btns) {
            if (b->text() == text) { return b; }
        }
        return nullptr;
    }

private slots:

    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── section 9: the +TNF button is live ────────────────────────────────

    void tnf_add_button_is_live()
    {
        PanelHarness h;
        QPushButton* btn =
            h.panel->findChild<QPushButton*>(QStringLiteral("tnfAddButton"));
        QVERIFY(btn);
        QCOMPARE(btn->text(), QStringLiteral("+TNF"));
        QVERIFY(btn->isEnabled());
        QVERIFY2(!btn->toolTip().contains(QStringLiteral("NYI")),
                 qPrintable(btn->toolTip()));
    }

    // A control drawn on a pan acts on THAT pan, so the click has to name
    // it. Same contract as addRxClicked, which already carries m_panId.
    void tnf_add_button_reports_its_own_pan()
    {
        PanelHarness h;
        h.panel->setPanId(QStringLiteral("pan-2"));
        QPushButton* btn =
            h.panel->findChild<QPushButton*>(QStringLiteral("tnfAddButton"));
        QVERIFY(btn);

        QSignalSpy spy(h.panel, &SpectrumOverlayPanel::addTnfClicked);
        btn->click();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-2"));
    }

    // Design section 9: "There is a third stub to retire in the same work
    // ... Replace it rather than shipping both."
    void disabled_mnf_twin_is_gone()
    {
        PanelHarness h;
        QVERIFY2(buttonWithText(h.host, QStringLiteral("MNF")) == nullptr,
                 "the disabled MNF stub must be replaced by +TNF, not "
                 "shipped beside it");
    }

    // ── section 7.5: +TNF centre = VFO + RIT, shifted into the sideband ───

    // USB F5 default passband is 100..3000 Hz, so notchSidebandShift returns
    // 100 + (2900 / 2) = 1550 and the notch lands mid-sideband, not on the
    // suppressed carrier.
    void tnf_add_centre_shifts_into_the_usb_sideband()
    {
        const double centre = NotchModel::tnfAddCenterHz(14200000.0, 100, 3000);
        QCOMPARE(centre, 14200000.0 + 1550.0);
    }

    // LSB mirrors it: -3000..-100 puts the middle at -1550 Hz.
    void tnf_add_centre_shifts_into_the_lsb_sideband()
    {
        const double centre = NotchModel::tnfAddCenterHz(7100000.0, -3000, -100);
        QCOMPARE(centre, 7100000.0 - 1550.0);
    }

    // The RIT term is the deliberate section 7.5 divergence. Thetis scales
    // RITValue by 1e-6 onto a Hz quantity, which makes 100 Hz of RIT move
    // the notch by 0.0001 Hz: a unit bug, not a behavioural choice. Section
    // 4.1 puts RIT inside the WDSP shift, so the demodulated RF already
    // carries it and a notch at bare VFO would sit rit_hz off the signal.
    // SliceModel::effectiveRxFrequency is the existing VFO+RIT accessor, so
    // the caller feeds that in.
    void tnf_add_centre_carries_rit_because_the_shift_does()
    {
        SliceModel slice;
        slice.setFrequency(14200000.0);
        slice.setFilterLow(100);
        slice.setFilterHigh(3000);
        slice.setRitEnabled(true);
        slice.setRitHz(250);

        const double centre = NotchModel::tnfAddCenterHz(
            slice.effectiveRxFrequency(), slice.filterLow(), slice.filterHigh());
        QCOMPARE(centre, 14200000.0 + 250.0 + 1550.0);
    }

    // ── section 7: the status-bar TNF light follows the global flag ───────

    void tnf_indicator_lights_accent_when_enabled()
    {
        const QString on = MainWindow::tnfIndicatorStyleSheet(true, 0);
        QVERIFY2(on.contains(QLatin1String(Style::kAccent)), qPrintable(on));
        QVERIFY2(on.contains(QStringLiteral("font-size: 11px")), qPrintable(on));
        QVERIFY2(!on.contains(QStringLiteral("line-through")), qPrintable(on));

        // Notches present changes nothing while the master switch is on.
        QCOMPARE(MainWindow::tnfIndicatorStyleSheet(true, 4), on);
    }

    // Maintainer decision D-a ships the master enable OFF, so the first
    // notch an operator places does nothing until they turn it on. The OFF
    // state therefore has to be unmistakable rather than a subtle tint:
    // struck-through in every off state, and escalated to the warning amber
    // the moment notches actually exist and are being bypassed.
    void tnf_indicator_off_state_is_unmistakable()
    {
        const QString idle = MainWindow::tnfIndicatorStyleSheet(false, 0);
        QVERIFY2(idle.contains(QStringLiteral("line-through")), qPrintable(idle));
        QVERIFY2(!idle.contains(QLatin1String(Style::kAccent)), qPrintable(idle));
        QVERIFY2(idle.contains(QStringLiteral("font-size: 11px")), qPrintable(idle));

        const QString bypassing = MainWindow::tnfIndicatorStyleSheet(false, 1);
        QVERIFY2(bypassing.contains(QStringLiteral("line-through")),
                 qPrintable(bypassing));
        QVERIFY2(bypassing.contains(QLatin1String(Style::kAmberWarn)),
                 qPrintable(bypassing));
        QVERIFY2(bypassing != idle,
                 "notches that exist but are bypassed must not read the same "
                 "as no notches at all: that is the D-a hazard state");
    }

    // The strikethrough is only unmistakable if Qt actually draws it. Qt's
    // stylesheet style implements text-decoration by flipping the resolved
    // font's strikeOut flag, so this asserts the property survives polish
    // rather than assuming the declaration is honoured.
    void tnf_indicator_strikethrough_reaches_the_font()
    {
        QLabel label(QStringLiteral("TNF"));

        label.setStyleSheet(MainWindow::tnfIndicatorStyleSheet(false, 1));
        label.ensurePolished();
        QVERIFY2(label.font().strikeOut(),
                 "the off state's line-through did not reach the font, so "
                 "the operator sees only a colour change");

        label.setStyleSheet(MainWindow::tnfIndicatorStyleSheet(true, 1));
        label.ensurePolished();
        QVERIFY2(!label.font().strikeOut(),
                 "the on state must not be struck through");
    }

    // #404858 is the literal the CWX / DVK / FDX labels beside it already
    // use in buildStatusBar(), so the four stay a matched set while TNF is
    // off and idle.
    void tnf_indicator_dims_to_its_siblings_when_idle()
    {
        const QString idle = MainWindow::tnfIndicatorStyleSheet(false, 0);
        QVERIFY2(idle.contains(QStringLiteral("#404858")), qPrintable(idle));
    }

    void tnf_tooltip_reports_an_empty_notch_list()
    {
        const QString tip = MainWindow::tnfIndicatorTooltip(0, false);
        QVERIFY2(tip.contains(QStringLiteral("no notches")), qPrintable(tip));
        QVERIFY2(tip.contains(QStringLiteral("Click to toggle")), qPrintable(tip));
    }

    void tnf_tooltip_reports_count_and_state()
    {
        const QString many = MainWindow::tnfIndicatorTooltip(3, true);
        QVERIFY2(many.contains(QStringLiteral("3 notches")), qPrintable(many));
        QVERIFY2(many.contains(QStringLiteral("enabled")), qPrintable(many));

        const QString one = MainWindow::tnfIndicatorTooltip(1, false);
        QVERIFY2(one.contains(QStringLiteral("1 notch,")), qPrintable(one));
        QVERIFY2(one.contains(QStringLiteral("bypassed")), qPrintable(one));
    }

    // ── section 10.2: fixed accelerator, because there is nothing to
    //    register with (KeyboardSetupPages.cpp is a 100% NYI stub and no
    //    ShortcutManager exists anywhere in src/) ───────────────────────

    void tnf_toggle_accelerator_is_fixed_and_unclaimed()
    {
        QCOMPARE(MainWindow::tnfToggleShortcut(),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));

        // Every accelerator MainWindow already hands out. A collision means
        // two menu items fight for the same chord and Qt fires neither
        // (QAction::ambiguous). Read out of MainWindow.cpp on this base:
        // 5111, 5135, 5150, 5208, 5283, 5298, 5422, 5976, 5989, 6029, 6048,
        // 6179. setShortcut appears in no other file under src/.
        const QList<QKeySequence> taken = {
            QKeySequence(Qt::CTRL | Qt::Key_Comma),
            QKeySequence(Qt::CTRL | Qt::Key_Q),
            QKeySequence(Qt::CTRL | Qt::Key_K),
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
            QKeySequence(QStringLiteral("Ctrl+L")),
            QKeySequence(QStringLiteral("Ctrl+R")),
            QKeySequence(Qt::CTRL | Qt::Key_M),
            QKeySequence(QStringLiteral("Ctrl+Shift+S")),
            QKeySequence(QStringLiteral("Ctrl+Shift+R")),
            QKeySequence(QStringLiteral("Ctrl+Shift+D")),
            QKeySequence(Qt::CTRL | Qt::Key_X),
            QKeySequence(QStringLiteral("Ctrl+Shift+K")),
        };
        QVERIFY2(!taken.contains(MainWindow::tnfToggleShortcut()),
                 "TNF accelerator collides with one MainWindow already "
                 "registers");
    }

    // Mirrors the two connects buildMenuBar() makes between DSP > TNF and
    // NotchModel, and the refresh buildStatusBar() makes between the light
    // and the same signal, the way tst_applet_visibility_menu_wiring.cpp
    // mirrors its menus: MainWindow cannot be instantiated in a unit test,
    // but the model, the QAction and the indicator statics here are the
    // production types.
    //
    // The bug this pins is the echo loop. Without the QSignalBlocker,
    // setChecked re-emits toggled, which writes the model again, so one
    // operator gesture reaches NotchModel twice.
    void tnf_menu_action_and_light_stay_in_sync_without_recursion()
    {
        NotchModel notches;
        QAction action;
        action.setCheckable(true);
        const bool initial = notches.globalEnabled();
        QCOMPARE(initial, false);          // maintainer decision D-a
        action.setChecked(initial);

        // The status-bar light, standing in for the production QLabel.
        QString lightStyle =
            MainWindow::tnfIndicatorStyleSheet(initial, 0);
        QString lightTip =
            MainWindow::tnfIndicatorTooltip(0, initial);

        QObject::connect(&action, &QAction::toggled,
                         &notches, &NotchModel::setGlobalEnabled);
        QObject::connect(&notches, &NotchModel::globalEnabledChanged,
                         &action, [&action](bool on) {
            QSignalBlocker b(&action);
            action.setChecked(on);
        });
        QObject::connect(&notches, &NotchModel::globalEnabledChanged,
                         &action, [&](bool) {
            const int count = static_cast<int>(notches.notches().size());
            lightStyle = MainWindow::tnfIndicatorStyleSheet(
                notches.globalEnabled(), count);
            lightTip = MainWindow::tnfIndicatorTooltip(
                count, notches.globalEnabled());
        });

        QSignalSpy modelSpy(&notches, &NotchModel::globalEnabledChanged);
        QSignalSpy actionSpy(&action, &QAction::toggled);

        action.toggle();                          // operator picks the item
        QCOMPARE(notches.globalEnabled(), !initial);
        QCOMPARE(modelSpy.count(), 1);            // reached the model once
        QCOMPARE(actionSpy.count(), 1);
        QVERIFY2(lightStyle.contains(QLatin1String(Style::kAccent)),
                 qPrintable(lightStyle));
        QVERIFY2(lightTip.contains(QStringLiteral("Click to toggle")),
                 qPrintable(lightTip));

        notches.setGlobalEnabled(initial);        // status-bar light, or TCI
        QCOMPARE(action.isChecked(), initial);
        QCOMPARE(modelSpy.count(), 2);
        QCOMPARE(actionSpy.count(), 1);           // blocker held the echo
        QVERIFY2(lightStyle.contains(QStringLiteral("line-through")),
                 qPrintable(lightStyle));
    }

    // ── the five MainWindow entry points this task owns ───────────────────

    // MainWindow cannot be stood up in a unit test (it boots WDSP, the audio
    // engine and the discovery thread), so its entry points are resolved by
    // name, the same seam tst_notch_hit_test.cpp uses for the fan-out slots.
    //
    // Slots, not lambdas: ensureOverlayPanels re-runs on every
    // PanadapterStack::countChanged, and Qt6 silently ignores
    // Qt::UniqueConnection when the target is a lambda, so a lambda would
    // stack one extra connection per layout switch and add one extra notch
    // per +TNF press.
    void mainwindow_exposes_the_tnf_control_slots()
    {
        const QMetaObject& mo = MainWindow::staticMetaObject;
        for (const char* sig : {"onAddTnfClicked(QString)",
                                "onNotchAddRejected(QString)",
                                "refreshTnfIndicator()"}) {
            QVERIFY2(mo.indexOfSlot(sig) >= 0,
                     qPrintable(QStringLiteral("MainWindow::%1 is not an "
                                               "invokable slot; the TNF "
                                               "control wiring would be "
                                               "unmade or would silently "
                                               "duplicate")
                                    .arg(QLatin1String(sig))));
        }
    }

    // ── correction 16: a rejected add is not silent ───────────────────────

    // Without this, a +TNF press inside the 10 Hz dedupe window does nothing
    // at all and the operator has no way to tell the button from a dead one.
    void rejected_add_reaches_operator_visible_feedback()
    {
        NotchModel notches;
        QSignalSpy rejects(&notches, &NotchModel::notchAddRejected);

        QVERIFY(notches.addNotch(14200000.0) > 0);
        QCOMPARE(rejects.count(), 0);

        // Inside the 10 Hz window (console.cs:40259-40260 [v2.10.3.15]).
        QCOMPARE(notches.addNotch(14200005.0), -1);
        QCOMPARE(rejects.count(), 1);

        const QString reason = rejects.at(0).at(0).toString();
        QVERIFY2(!reason.isEmpty(), "the reject must carry a reason to show");

        // The production wiring hands that reason to a toast; the text it
        // renders is a pure function so the wording can be pinned here.
        const QString notice = MainWindow::tnfAddRejectedNotice(reason);
        QVERIFY2(notice.contains(reason), qPrintable(notice));
        QVERIFY2(notice.contains(QStringLiteral("Notch"), Qt::CaseInsensitive),
                 qPrintable(notice));
    }
};

QTEST_MAIN(TestTnfUiWiring)
#include "tst_tnf_ui_wiring.moc"
