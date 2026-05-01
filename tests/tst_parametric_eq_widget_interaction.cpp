// =================================================================
// tests/tst_parametric_eq_widget_interaction.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.  Cites for the
// interaction logic under test live in ParametricEqWidget.cpp.
// =================================================================
//
// Phase 3M-3a-ii follow-up Batch 4 -- mouse + wheel + signal
// emission verification.  Pins:
//   * Left-click on a band selects it (and emits selectedIndexChanged
//     + pointSelected with the right payload).
//   * Drag-move with the mouse re-emits pointsChanged(true) +
//     pointDataChanged(true), and snapping a new dB/Hz round-trips
//     through dbFromY/freqFromX.
//   * Endpoint drag is gain-only (frequency stays pinned).
//   * Right-click is ignored (no signals fire).
//   * Wheel without modifier multiplies Q by 1.12^steps.
//   * Wheel + Shift adjusts gain by 0.5 dB per step.
//   * Wheel + Ctrl shifts frequency by chooseFrequencyStep/5 (>=1 Hz).
//   * Click on the global gain handle starts a drag (later wheel/
//     mouseMove events use that path instead of the band-handle path).
//   * Mouse release fires the final non-dragging "commit" emission.
//   * Click on empty area deselects (selectedIndexChanged + pointUnselected).
//
// Hand-computed expected values trace each branch back to the C#
// source in ucParametricEq.cs [v2.10.3.13].
//
// =================================================================

#include "../src/gui/widgets/ParametricEqWidget.h"

#include <QApplication>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

#include <cmath>

// Tester shim: friended in ParametricEqWidget.h so this subclass can
// reach private state for hand-computed expecteds.  We keep public
// access tight: only what the test needs.
class ParametricEqInteractionTester : public NereusSDR::ParametricEqWidget {
public:
    using NereusSDR::ParametricEqWidget::ParametricEqWidget;

    // Promote the math + ordering helpers so we can hand-compute pixel
    // positions and dB/Hz round-trips precisely.
    using NereusSDR::ParametricEqWidget::computePlotRect;
    using NereusSDR::ParametricEqWidget::xFromFreq;
    using NereusSDR::ParametricEqWidget::yFromDb;
    using NereusSDR::ParametricEqWidget::freqFromX;
    using NereusSDR::ParametricEqWidget::dbFromY;
    using NereusSDR::ParametricEqWidget::chooseFrequencyStep;
    using NereusSDR::ParametricEqWidget::indexFromBandId;

    // Direct member access for assertions.
    QVector<EqPoint>& pointsMut()                  { return m_points; }
    const QVector<EqPoint>& pointsConst()  const   { return m_points; }
    int  selectedIndex()                  const   { return m_selectedIndex; }
    int  dragIndex()                      const   { return m_dragIndex; }
    bool draggingPoint()                  const   { return m_draggingPoint; }
    bool draggingGlobalGain()             const   { return m_draggingGlobalGain; }
    double globalGainDb()                 const   { return m_globalGainDb; }
    double frequencyMinHz()               const   { return m_frequencyMinHz; }
    double frequencyMaxHz()               const   { return m_frequencyMaxHz; }
    double dbMin()                        const   { return m_dbMin; }
    double dbMax()                        const   { return m_dbMax; }
    double qMin()                         const   { return m_qMin; }
    double qMax()                         const   { return m_qMax; }

    // Force a known starting state for deterministic tests.
    void replaceWithTwoBands(double f0, double f1) {
        m_points.clear();
        EqPoint p0;
        p0.bandId = 1;
        p0.frequencyHz = f0;
        p0.gainDb = 0.0;
        p0.q = 4.0;
        m_points.append(p0);

        EqPoint p1;
        p1.bandId = 2;
        p1.frequencyHz = f1;
        p1.gainDb = 0.0;
        p1.q = 4.0;
        m_points.append(p1);

        m_selectedIndex = -1;
        m_dragIndex = -1;
        m_draggingPoint = false;
        m_draggingGlobalGain = false;
    }
};

class TestParametricEqInteraction : public QObject {
    Q_OBJECT
private slots:
    void leftClickOnBandSelectsIt();
    void leftDragMovesBandFreqAndGain();
    void leftDragOnEndpointMovesGainOnly();
    void rightClickIsIgnored();
    void wheelNoModifierMultipliesQ();
    void wheelShiftAdjustsGainOnly();
    void wheelCtrlAdjustsFrequencyOnly();
    void clickOnGlobalGainHandleStartsDrag();
    void releaseFiresFinalNonDraggingSignal();
    void clickOnEmptyAreaDeselects();
};

// =================================================================
// Helpers.
// =================================================================

// Synth a wheel event with the given delta + modifiers and dispatch it
// to the widget.  Uses Qt6 QWheelEvent constructor (8-arg form).
static void sendWheel(QWidget* w, QPoint pos, int angleDeltaY, Qt::KeyboardModifiers mods) {
    QWheelEvent we(
        QPointF(pos),
        w->mapToGlobal(QPointF(pos)),
        QPoint(0, 0),
        QPoint(0, angleDeltaY),
        Qt::NoButton,
        mods,
        Qt::NoScrollPhase,
        false
    );
    QApplication::sendEvent(w, &we);
}

// =================================================================
// Tests.
// =================================================================

// Click directly on a band's dot -> selectedIndex = that band, signals fire
// in order (pointSelected + selectedIndexChanged), and dragging starts.
void TestParametricEqInteraction::leftClickOnBandSelectsIt() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    // The default reset gives 10 bands, all gain=0.  Pick band index 5
    // (mid-range) so it's safely interior.
    QVERIFY(w.pointsConst().size() == 10);

    QSignalSpy spySel(&w, &NereusSDR::ParametricEqWidget::pointSelected);
    QSignalSpy spyIdxCh(&w, &NereusSDR::ParametricEqWidget::selectedIndexChanged);

    QRect plot = w.computePlotRect();
    int idx = 5;
    const auto& p = w.pointsConst().at(idx);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, dot);

    QCOMPARE(w.selectedIndex(), idx);
    QCOMPARE(w.dragIndex(),     idx);
    QVERIFY(w.draggingPoint());

    QCOMPARE(spySel.count(),   1);
    QCOMPARE(spyIdxCh.count(), 1);

    // pointSelected payload = (idx, bandId, freq, gain, q).
    auto sel = spySel.takeFirst();
    QCOMPARE(sel.at(0).toInt(),    idx);
    QCOMPARE(sel.at(1).toInt(),    p.bandId);
    QCOMPARE(sel.at(2).toDouble(), p.frequencyHz);
    QCOMPARE(sel.at(3).toDouble(), p.gainDb);
    QCOMPARE(sel.at(4).toDouble(), p.q);

    // selectedIndexChanged payload = (false) -- not dragging during the
    // setSelectedIndex setter call (mouse-press dragging flag flips
    // *after* setSelectedIndex returns).
    auto idxCh = spyIdxCh.takeFirst();
    QCOMPARE(idxCh.at(0).toBool(), false);

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, dot);
}

// Drag a band's dot to a new (x, y) -> frequency + gain update, signals
// fire on every move with isDragging=true; mouse-up emits final non-
// dragging commit.
void TestParametricEqInteraction::leftDragMovesBandFreqAndGain() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    // Start with 2 bands so any selected interior band is impossible
    // (only endpoints exist).  Use a 3-band setup where we can drag
    // the middle one.
    w.pointsMut().clear();
    {
        NereusSDR::ParametricEqWidget::EqPoint p0;
        p0.bandId = 1; p0.frequencyHz = 0.0;    p0.gainDb = 0.0; p0.q = 4.0;
        NereusSDR::ParametricEqWidget::EqPoint p1;
        p1.bandId = 2; p1.frequencyHz = 2000.0; p1.gainDb = 0.0; p1.q = 4.0;
        NereusSDR::ParametricEqWidget::EqPoint p2;
        p2.bandId = 3; p2.frequencyHz = 4000.0; p2.gainDb = 0.0; p2.q = 4.0;
        w.pointsMut().append(p0);
        w.pointsMut().append(p1);
        w.pointsMut().append(p2);
    }

    QRect plot = w.computePlotRect();
    int idx = 1; // middle, interior band
    const auto& p = w.pointsConst().at(idx);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    // Press first to start the drag.
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, dot);
    QVERIFY(w.draggingPoint());

    QSignalSpy spyPC  (&w, &NereusSDR::ParametricEqWidget::pointsChanged);
    QSignalSpy spyPDC (&w, &NereusSDR::ParametricEqWidget::pointDataChanged);

    // Move 30px right (freq up) and 20px down (gain down).
    QPoint moved(dot.x() + 30, dot.y() + 20);
    double expectedFreq = w.freqFromX(plot, moved.x());
    double expectedGain = w.dbFromY (plot, moved.y());

    QTest::mouseMove(&w, moved);

    // The drag fires pointsChanged(true) + pointDataChanged(true).
    QVERIFY(spyPC.count() >= 1);
    QVERIFY(spyPDC.count() >= 1);

    // Last emission carries isDragging=true.
    QCOMPARE(spyPC.last().at(0).toBool(),  true);
    QCOMPARE(spyPDC.last().at(5).toBool(), true);

    // Frequency and gain match dbFromY/freqFromX (within float tolerance).
    const auto& curP = w.pointsConst().at(w.dragIndex());
    QVERIFY2(qAbs(curP.frequencyHz - expectedFreq) < 1.0,
             qPrintable(QString("freq=%1 expected=%2").arg(curP.frequencyHz).arg(expectedFreq)));
    QVERIFY2(qAbs(curP.gainDb - expectedGain) < 0.2,
             qPrintable(QString("gain=%1 expected=%2").arg(curP.gainDb).arg(expectedGain)));

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);
}

// Drag the LEFT endpoint -> gain changes, frequency stays pinned at
// m_frequencyMinHz (isFrequencyLockedIndex returns true for index 0).
void TestParametricEqInteraction::leftDragOnEndpointMovesGainOnly() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    int idx = 0;  // first point, locked endpoint
    const double pinnedF = w.pointsConst().at(idx).frequencyHz;
    QCOMPARE(pinnedF, w.frequencyMinHz());

    QRect plot = w.computePlotRect();
    QPoint dot(qRound(w.xFromFreq(plot, pinnedF)),
               qRound(w.yFromDb (plot, w.pointsConst().at(idx).gainDb)));

    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, dot);
    QCOMPARE(w.dragIndex(), 0);

    // Move 50 px right + 30 px up.
    QPoint moved(dot.x() + 50, dot.y() - 30);
    QTest::mouseMove(&w, moved);

    // Frequency is unchanged (locked).
    QCOMPARE(w.pointsConst().at(idx).frequencyHz, pinnedF);

    // Gain went UP because we moved up on screen (smaller y -> higher dB).
    QVERIFY2(w.pointsConst().at(idx).gainDb > 0.0,
             qPrintable(QString("expected gain > 0 after upward drag, got %1")
                            .arg(w.pointsConst().at(idx).gainDb)));

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);
}

// Right-click does nothing -- no signals fire and selection stays put.
void TestParametricEqInteraction::rightClickIsIgnored() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QSignalSpy spySel  (&w, &NereusSDR::ParametricEqWidget::pointSelected);
    QSignalSpy spyUnsel(&w, &NereusSDR::ParametricEqWidget::pointUnselected);
    QSignalSpy spyIdxCh(&w, &NereusSDR::ParametricEqWidget::selectedIndexChanged);

    QRect plot = w.computePlotRect();
    const auto& p = w.pointsConst().at(3);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    QTest::mouseClick(&w, Qt::RightButton, Qt::NoModifier, dot);

    QCOMPARE(w.selectedIndex(), -1);   // still nothing selected
    QCOMPARE(spySel.count(),    0);
    QCOMPARE(spyUnsel.count(),  0);
    QCOMPARE(spyIdxCh.count(),  0);
    QVERIFY(!w.draggingPoint());
}

// Wheel without modifier on the selected band -> Q multiplied by 1.12^steps.
// Hand-computed: q starts at 4.0, +1 step -> 4.0 * 1.12 = 4.48.
void TestParametricEqInteraction::wheelNoModifierMultipliesQ() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    int idx = 4;  // interior band, q=4.0 from default reset
    QCOMPARE(w.pointsConst().at(idx).q, 4.0);

    // Select first.
    QRect plot = w.computePlotRect();
    const auto& p = w.pointsConst().at(idx);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    w.setSelectedIndex(idx);
    QCOMPARE(w.selectedIndex(), idx);

    QSignalSpy spyPC (&w, &NereusSDR::ParametricEqWidget::pointsChanged);
    QSignalSpy spyPDC(&w, &NereusSDR::ParametricEqWidget::pointDataChanged);

    sendWheel(&w, dot, /*angleDeltaY=*/120, Qt::NoModifier);

    double expectedQ = 4.0 * 1.12;  // steps=120/120=1; 1.12^1 = 1.12.
    QVERIFY2(qAbs(w.pointsConst().at(idx).q - expectedQ) < 1e-6,
             qPrintable(QString("q=%1 expected %2").arg(w.pointsConst().at(idx).q).arg(expectedQ)));

    QCOMPARE(spyPC.count(),  1);
    QCOMPARE(spyPDC.count(), 1);
    // No drag in progress -> isDragging payload is false.
    QCOMPARE(spyPC.last().at(0).toBool(),  false);
    QCOMPARE(spyPDC.last().at(5).toBool(), false);
}

// Wheel + Shift on selected band -> gain += steps * 0.5.
void TestParametricEqInteraction::wheelShiftAdjustsGainOnly() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    int idx = 4;
    w.setSelectedIndex(idx);

    QRect plot = w.computePlotRect();
    const auto& p = w.pointsConst().at(idx);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    double startGain = p.gainDb;
    double startQ    = p.q;
    double startFreq = p.frequencyHz;

    sendWheel(&w, dot, /*angleDeltaY=*/240, Qt::ShiftModifier); // 2 steps

    // Expected: gain += 2 * 0.5 = +1.0.
    double expectedGain = startGain + 1.0;
    QVERIFY2(qAbs(w.pointsConst().at(idx).gainDb - expectedGain) < 1e-6,
             qPrintable(QString("gain=%1 expected %2")
                            .arg(w.pointsConst().at(idx).gainDb).arg(expectedGain)));

    // Q + freq unchanged.
    QCOMPARE(w.pointsConst().at(idx).q,           startQ);
    QCOMPARE(w.pointsConst().at(idx).frequencyHz, startFreq);
}

// Wheel + Ctrl on selected band -> frequency += steps * (chooseFrequencyStep/5).
// span = 4000 -> chooseFrequencyStep(4000) = 500 (s <= 6000), step = 100 Hz.
void TestParametricEqInteraction::wheelCtrlAdjustsFrequencyOnly() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    int idx = 4;  // interior, NOT locked
    w.setSelectedIndex(idx);

    QRect plot = w.computePlotRect();
    const auto& p = w.pointsConst().at(idx);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    double startFreq = p.frequencyHz;
    double startGain = p.gainDb;
    double startQ    = p.q;

    // Verify hand-computed step: chooseFrequencyStep(4000) = 500.
    QCOMPARE(w.chooseFrequencyStep(4000.0), 500.0);
    double stepHz = 500.0 / 5.0;  // = 100 Hz, well above the 1.0 floor

    sendWheel(&w, dot, /*angleDeltaY=*/120, Qt::ControlModifier); // +1 step

    // Expected: freq += 1 * 100 = +100 Hz.  But ordering rules may have
    // re-sorted the band; we look up by bandId to verify.
    int curIdx = w.indexFromBandId(p.bandId);
    QVERIFY(curIdx >= 0);
    double expectedFreq = startFreq + stepHz;
    QVERIFY2(qAbs(w.pointsConst().at(curIdx).frequencyHz - expectedFreq) < 1.0,
             qPrintable(QString("freq=%1 expected %2 (idx %3 -> %4)")
                            .arg(w.pointsConst().at(curIdx).frequencyHz)
                            .arg(expectedFreq).arg(idx).arg(curIdx)));

    // Gain + Q unchanged.
    QCOMPARE(w.pointsConst().at(curIdx).gainDb, startGain);
    QCOMPARE(w.pointsConst().at(curIdx).q,      startQ);
}

// Click ON the global gain handle starts a global drag (not a band drag).
// Subsequent mouse moves call setGlobalGainDb instead of moving a band.
void TestParametricEqInteraction::clickOnGlobalGainHandleStartsDrag() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QRect plot = w.computePlotRect();

    // Hand-compute the global handle position: matches drawGlobalGainHandle
    // (cs:2372-2393) -- triangle tip at (plot.right + handleXOffset, y(global)).
    // Default m_globalHandleXOffset=6, m_globalGainDb=0 -> y(0) is mid-plot.
    int handleX = plot.right() + 6 + 2;            // 2 px in from the tip (inside the body)
    int handleY = qRound(w.yFromDb(plot, 0.0));
    QPoint handle(handleX, handleY);

    double startGlobal = w.globalGainDb();
    QSignalSpy spyGGC(&w, &NereusSDR::ParametricEqWidget::globalGainChanged);

    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, handle);
    QVERIFY(w.draggingGlobalGain());
    QVERIFY(!w.draggingPoint());

    // Move down 40 px -> global gain decreases.
    QPoint moved(handle.x(), handle.y() + 40);
    QTest::mouseMove(&w, moved);

    QVERIFY2(w.globalGainDb() < startGlobal,
             qPrintable(QString("expected global gain to drop from %1, got %2")
                            .arg(startGlobal).arg(w.globalGainDb())));

    QVERIFY(spyGGC.count() >= 1);
    // Last drag emission carries isDragging=true.
    QCOMPARE(spyGGC.last().at(0).toBool(), true);

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);
}

// After a drag commits, mouse-up fires the final pointsChanged(false)
// and pointDataChanged(false) -- the "non-dragging" commit signal.
void TestParametricEqInteraction::releaseFiresFinalNonDraggingSignal() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    int idx = 4;  // interior
    QRect plot = w.computePlotRect();
    const auto& p = w.pointsConst().at(idx);
    QPoint dot(qRound(w.xFromFreq(plot, p.frequencyHz)),
               qRound(w.yFromDb (plot, p.gainDb)));

    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, dot);

    // Move enough to mark dirty.
    QPoint moved(dot.x() + 25, dot.y() + 15);
    QTest::mouseMove(&w, moved);

    QSignalSpy spyPC (&w, &NereusSDR::ParametricEqWidget::pointsChanged);
    QSignalSpy spyPDC(&w, &NereusSDR::ParametricEqWidget::pointDataChanged);

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);

    // Release fires exactly one pointsChanged(false) + one pointDataChanged(false).
    QCOMPARE(spyPC.count(),  1);
    QCOMPARE(spyPDC.count(), 1);
    QCOMPARE(spyPC.last().at(0).toBool(),  false);
    QCOMPARE(spyPDC.last().at(5).toBool(), false);

    // No longer dragging.
    QVERIFY(!w.draggingPoint());
}

// Click on empty area (inside the plot but away from any dot) -> selection
// drops to -1 and pointUnselected fires for the previously selected band.
void TestParametricEqInteraction::clickOnEmptyAreaDeselects() {
    ParametricEqInteractionTester w;
    w.resize(460, 320);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    int idx = 4;
    w.setSelectedIndex(idx);
    QCOMPARE(w.selectedIndex(), idx);

    QSignalSpy spyUnsel(&w, &NereusSDR::ParametricEqWidget::pointUnselected);
    QSignalSpy spyIdxCh(&w, &NereusSDR::ParametricEqWidget::selectedIndexChanged);

    QRect plot = w.computePlotRect();

    // Pick a point well away from any band dot: top-left of the plot
    // area (every band dot is on the y(0) line, this is high above it).
    QPoint emptySpot(plot.left() + 5, plot.top() + 5);

    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, emptySpot);

    QCOMPARE(w.selectedIndex(), -1);
    QCOMPARE(spyUnsel.count(), 1);
    QCOMPARE(spyIdxCh.count(), 1);

    auto unsel = spyUnsel.takeFirst();
    QCOMPARE(unsel.at(0).toInt(), idx);

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, emptySpot);
}

QTEST_MAIN(TestParametricEqInteraction)
#include "tst_parametric_eq_widget_interaction.moc"
