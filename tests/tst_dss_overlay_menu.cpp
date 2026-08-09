// =================================================================
// tests/tst_dss_overlay_menu.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// 3D stacked-trace spectrum plan, Task 13: the 3D VIEW control section.
// Design: docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md
// Plan:   docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md
//
// Pins SpectrumOverlayMenu's "3D VIEW" section: the Spectrum render-mode
// combo plus 3D Floor / 3D Gain / 3D Span / 3D Angle / 3D Slice Shadow,
// with upstream's exact labels, ranges, defaults and tooltip wording for
// the five ported controls (AetherSDR SpectrumOverlayMenu.cpp:1874-1943,
// :2250-2273 [@1872028c]) and the NereusSDR-original 3D Angle slider /
// 3D Slice Shadow checkbox (the latter's placement here rather than its
// own QMenu is a coordinator ruling -- see Task 12's row in
// docs/attribution/aethersdr-reconciliation.md).
//
// Six controls in total, so every test in this file that enumerates them
// enumerates six: mode combo, floor/gain/span/angle sliders, shadow check.
//
// The end-to-end wiring test at the bottom (sixControls_...) reuses the
// exact right-click idiom tst_notch_hit_test.cpp's
// overlay_menu_add_notch_forwards_the_cursor_frequency established for
// this same popup: synthesize a real QMouseEvent, let SpectrumWidget's own
// mousePressEvent construct and wire m_overlayMenu, then find it as a
// child and drive its widgets directly. That is the only way to prove the
// six connect() calls in SpectrumWidget.cpp are the REAL wiring rather
// than a parallel set of connects duplicated inside the test.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QMouseEvent>

#include "core/ConnectionState.h"
#include "gui/SpectrumOverlayMenu.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

// Mirrors tst_notch_hit_test.cpp's sendMouse(): QTest::mouseClick works for
// plain clicks, but this project's established idiom for driving
// SpectrumWidget's mousePressEvent in headless tests is a directly
// synthesized QMouseEvent via QApplication::sendEvent, so this file matches
// rather than introduces a second way to do the same thing.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, mods);
    QApplication::sendEvent(w, &me);
}

} // namespace

class TestDssOverlayMenu : public QObject {
    Q_OBJECT

private slots:
    void controlsExistWithUpstreamRangesAndDefaults() {
        SpectrumOverlayMenu m;
        m.resize(320, 640);
        m.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m));

        auto* mode   = m.findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"));
        auto* floor  = m.findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"));
        auto* gain   = m.findChild<QSlider*>(QStringLiteral("dssGainSlider"));
        auto* span   = m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"));
        auto* angle  = m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"));
        auto* shadow = m.findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"));

        QVERIFY(mode);
        QCOMPARE(mode->count(), 2);
        QCOMPARE(mode->currentIndex(), 0);  // DEFAULTS: mode 2D

        QVERIFY(floor);
        QCOMPARE(floor->minimum(), 0);
        QCOMPARE(floor->maximum(), 24);
        QCOMPARE(floor->value(), 6);        // DEFAULTS: 3D Floor 6

        QVERIFY(gain);
        QCOMPARE(gain->minimum(), 0);
        QCOMPARE(gain->maximum(), 100);
        QCOMPARE(gain->value(), 70);        // DEFAULTS: 3D Gain 70

        QVERIFY(span);
        QCOMPARE(span->minimum(), 0);
        QCOMPARE(span->maximum(), 100);
        QCOMPARE(span->value(), 100);       // DEFAULTS: 3D Span 100

        QVERIFY(angle);
        QCOMPARE(angle->minimum(), 0);
        QCOMPARE(angle->maximum(), 100);
        QCOMPARE(angle->value(), 50);       // DEFAULTS: 3D Angle 50 (load-bearing)

        QVERIFY(shadow);
        QCOMPARE(shadow->isChecked(), false); // DEFAULTS: Slice Shadow off
    }

    void movingASlider_emitsItsSignal() {
        SpectrumOverlayMenu m;
        m.resize(320, 640);
        m.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m));

        QSignalSpy modeSpy(&m, &SpectrumOverlayMenu::spectrumRenderModeChanged);
        QSignalSpy floorSpy(&m, &SpectrumOverlayMenu::dssFloorDepthChanged);
        QSignalSpy gainSpy(&m, &SpectrumOverlayMenu::dssGainChanged);
        QSignalSpy spanSpy(&m, &SpectrumOverlayMenu::dssRowSpanChanged);
        QSignalSpy angleSpy(&m, &SpectrumOverlayMenu::dssAngleChanged);
        QSignalSpy shadowSpy(&m, &SpectrumOverlayMenu::dssSliceShadowChanged);

        m.findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"))->setCurrentIndex(1);
        m.findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"))->setValue(12);
        m.findChild<QSlider*>(QStringLiteral("dssGainSlider"))->setValue(33);
        m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"))->setValue(44);
        m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->setValue(80);
        m.findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"))->setChecked(true);

        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(modeSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(floorSpy.count(), 1);
        QCOMPARE(floorSpy.at(0).at(0).toInt(), 12);
        QCOMPARE(gainSpy.count(), 1);
        QCOMPARE(gainSpy.at(0).at(0).toInt(), 33);
        QCOMPARE(spanSpy.count(), 1);
        QCOMPARE(spanSpy.at(0).at(0).toInt(), 44);
        QCOMPARE(angleSpy.count(), 1);
        QCOMPARE(angleSpy.at(0).at(0).toInt(), 80);
        QCOMPARE(shadowSpy.count(), 1);
        QCOMPARE(shadowSpy.at(0).at(0).toBool(), true);
    }

    // setDssValues seeds the widgets before showing without echoing back
    // out, or opening the menu would rewrite the operator's settings.
    // Extended from the brief's 5-arg skeleton to 6: Step 3 of the brief is
    // explicit that setDssValues "takes the shadow state too, and must not
    // echo", so the checkbox needs the same seed-without-echo proof as the
    // five sliders/combo -- otherwise every popup would silently reset
    // Slice Shadow to unchecked regardless of the operator's real setting.
    void setDssValues_doesNotEcho() {
        SpectrumOverlayMenu m;
        m.resize(320, 640);
        m.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m));

        QSignalSpy modeSpy(&m, &SpectrumOverlayMenu::spectrumRenderModeChanged);
        QSignalSpy floorSpy(&m, &SpectrumOverlayMenu::dssFloorDepthChanged);
        QSignalSpy gainSpy(&m, &SpectrumOverlayMenu::dssGainChanged);
        QSignalSpy spanSpy(&m, &SpectrumOverlayMenu::dssRowSpanChanged);
        QSignalSpy angleSpy(&m, &SpectrumOverlayMenu::dssAngleChanged);
        QSignalSpy shadowSpy(&m, &SpectrumOverlayMenu::dssSliceShadowChanged);

        m.setDssValues(1, 10, 40, 60, 25, true);

        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(floorSpy.count(), 0);
        QCOMPARE(gainSpy.count(), 0);
        QCOMPARE(spanSpy.count(), 0);
        QCOMPARE(angleSpy.count(), 0);
        QCOMPARE(shadowSpy.count(), 0);

        QCOMPARE(m.findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"))->currentIndex(), 1);
        QCOMPARE(m.findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"))->value(), 10);
        QCOMPARE(m.findChild<QSlider*>(QStringLiteral("dssGainSlider"))->value(), 40);
        QCOMPARE(m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"))->value(), 60);
        QCOMPARE(m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->value(), 25);
        QCOMPARE(m.findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"))->isChecked(), true);
    }

    void spanUnsupported_disablesTheSliderNotTheRest() {
        SpectrumOverlayMenu m;
        m.resize(320, 640);
        m.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m));

        m.setDssRowSpanSupported(false);
        QVERIFY(!m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"))->isEnabled());
        QVERIFY(m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->isEnabled());
        QVERIFY(m.findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"))->isEnabled());
    }

    // setDssRowSpanSupported's own upstream body keeps two distinct tooltip
    // strings so "the enabled and unavailable wordings cannot drift apart"
    // (upstream's own comment, AetherSDR SpectrumOverlayMenu.cpp:1931-1932
    // [@1872028c]) -- this proves the switch actually happens rather than
    // one wording silently winning both states.
    void spanUnsupported_tooltipDiffersFromSupported() {
        SpectrumOverlayMenu m;
        m.resize(320, 640);
        m.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m));

        auto* span = m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"));
        QVERIFY(span);
        const QString supportedTip = span->toolTip();
        QVERIFY(!supportedTip.isEmpty());

        m.setDssRowSpanSupported(false);
        const QString unsupportedTip = span->toolTip();
        QVERIFY(!unsupportedTip.isEmpty());
        QVERIFY(unsupportedTip != supportedTip);
        QVERIFY2(unsupportedTip.contains(QStringLiteral("CPU fallback")),
                 "unsupported tooltip should explain the CPU-fallback reason");
    }

    // -- end-to-end wiring (required verification #2: six controls, six
    // assertions) --------------------------------------------------------
    // A real right-click on SpectrumWidget, exactly like
    // tst_notch_hit_test.cpp's overlay_menu_add_notch_forwards_the_cursor_
    // frequency, so this exercises the SIX connect() calls actually
    // committed in SpectrumWidget.cpp rather than a parallel set of
    // connects re-declared inside the test (which would pass even if the
    // real wiring were missing or pointed at the wrong setter).
    void sixControls_wireThroughToSpectrumWidgetGetters() {
        SpectrumWidget w;
        w.resize(1000, 400);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setConnectionState(ConnectionState::Connected);
        w.setFrequencyRange(14'200'000.0, 128'000.0);

        // No notches/spots pushed, so a right-click on empty pan falls
        // through to the overlay menu (mirrors the notch test's own setup).
        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, 50),
                  Qt::RightButton, Qt::RightButton);

        auto* menu = w.findChild<SpectrumOverlayMenu*>();
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");
        menu->hide();

        auto* mode   = menu->findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"));
        auto* floor  = menu->findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"));
        auto* gain   = menu->findChild<QSlider*>(QStringLiteral("dssGainSlider"));
        auto* span   = menu->findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"));
        auto* angle  = menu->findChild<QSlider*>(QStringLiteral("dssAngleSlider"));
        auto* shadow = menu->findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"));
        QVERIFY(mode);
        QVERIFY(floor);
        QVERIFY(gain);
        QVERIFY(span);
        QVERIFY(angle);
        QVERIFY(shadow);

        // 1. Spectrum render mode.
        QCOMPARE(w.spectrumRenderMode(), 0);
        mode->setCurrentIndex(1);
        QCOMPARE(w.spectrumRenderMode(), 1);

        // 2. 3D Floor.
        floor->setValue(15);
        QCOMPARE(w.dssFloorDepth(), 15);

        // 3. 3D Gain.
        gain->setValue(22);
        QCOMPARE(w.dssGain(), 22);

        // 4. 3D Span.
        span->setValue(33);
        QCOMPARE(w.dssRowSpan(), 33);

        // 5. 3D Angle.
        angle->setValue(77);
        QCOMPARE(w.dssAngle(), 77);

        // 6. 3D Slice Shadow.
        QCOMPARE(w.threeDSliceDepth(), false);
        shadow->setChecked(true);
        QCOMPARE(w.threeDSliceDepth(), true);
    }
};

QTEST_MAIN(TestDssOverlayMenu)
#include "tst_dss_overlay_menu.moc"
