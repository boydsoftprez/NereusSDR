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
#include <limits>

// Tester shim: friended in ParametricEqWidget.h so this subclass can
// reach private state for hand-computed expecteds.  We keep public
// access tight: only what the test needs.
class ParametricEqInteractionTester : public NereusSDR::ParametricEqWidget {
public:
    using NereusSDR::ParametricEqWidget::ParametricEqWidget;

    // Promote the math + ordering helpers so we can hand-compute pixel
    // positions and dB/Hz round-trips precisely.
    using NereusSDR::ParametricEqWidget::computePlotRect;
    using NereusSDR::ParametricEqWidget::widthHandlePosition;
    using NereusSDR::ParametricEqWidget::isDraggingNow;
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
    void widthDragChangesOnlyQ();
    void widthHandleAtZeroAndNegativeGain_data();
    void widthHandleAtZeroAndNegativeGain();
    void clippedHandleDoesNotMutateQ();
    void clippedWidthDragIsGradualAndReturnsExactly();
    void widthDisabledWithoutQ();
    void gestureSignalsBracketFinalCommit();
    void crossingRetainsBandId();
    void externalLoadCancelsDrag();
    void runtimeStateRestoresExactValuesAndIds();
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

    // Pin signal-emit order: downstream observers (future TxCfcDialog
    // CFC-redispatch) depend on pointsChanged firing before
    // pointDataChanged.  See mouseReleaseEvent body cs:1803-1844 /
    // ParametricEqWidget.cpp:1788-1822.
    //
    // This drag setup only marks m_dragDirtyPoint (no global-drag, no
    // mid-drag reorder), so only the first if-block fires:
    //   1. raisePointsChanged(false)
    //   2. raisePointDataChanged(dragIdx, ..., false)
    // wasDraggingGlobal && globalDirty -> false (band drag, not global).
    // selectedDirty -> false (no mid-drag reorder triggered the
    // m_dragDirtySelectedIndex flag).  A 3-band reorder test that
    // exercises selectedDirty's release path is queued as Task 5.
    QStringList emitOrder;
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::pointsChanged,
                     [&]() { emitOrder << QStringLiteral("pointsChanged"); });
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::pointDataChanged,
                     [&]() { emitOrder << QStringLiteral("pointDataChanged"); });
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::globalGainChanged,
                     [&]() { emitOrder << QStringLiteral("globalGainChanged"); });
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::selectedIndexChanged,
                     [&]() { emitOrder << QStringLiteral("selectedIndexChanged"); });

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);

    // Release fires exactly one pointsChanged(false) + one pointDataChanged(false).
    QCOMPARE(spyPC.count(),  1);
    QCOMPARE(spyPDC.count(), 1);
    QCOMPARE(spyPC.last().at(0).toBool(),  false);
    QCOMPARE(spyPDC.last().at(5).toBool(), false);

    // Order matches mouseReleaseEvent body cs:1803-1844 /
    // ParametricEqWidget.cpp:1788-1822: pointsChanged before
    // pointDataChanged.  No globalGainChanged / selectedIndexChanged
    // fired because their dirty flags weren't set in this drag scenario.
    QCOMPARE(emitOrder, (QStringList{
        QStringLiteral("pointsChanged"),
        QStringLiteral("pointDataChanged"),
    }));

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

    // Capture the previously-selected band's full data BEFORE the empty-
    // area click so we can verify the pointUnselected payload reflects
    // that band, NOT the empty-click position.  A regression that emitted
    // pointUnselected(idx, 0, 0.0, 0.0, 0.0) (zeroed payload) would still
    // pass an index-only assertion.
    const auto& prevP = w.pointsConst().at(idx);
    int    prevBandId = prevP.bandId;
    double prevFreq   = prevP.frequencyHz;
    double prevGain   = prevP.gainDb;
    double prevQ      = prevP.q;

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

    // Verify ALL 5 payload args match the previously-selected band, not
    // the empty-click position.  raisePointUnselected uses the cached
    // oldPoint snapshot from setSelectedIndex (cs:1033-1039 /
    // ParametricEqWidget.cpp:1561-1564).
    auto unsel = spyUnsel.takeFirst();
    QCOMPARE(unsel.at(0).toInt(),    idx);
    QCOMPARE(unsel.at(1).toInt(),    prevBandId);
    QCOMPARE(unsel.at(2).toDouble(), prevFreq);
    QCOMPARE(unsel.at(3).toDouble(), prevGain);
    QCOMPARE(unsel.at(4).toDouble(), prevQ);

    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, emptySpot);
}

void TestParametricEqInteraction::widthDragChangesOnlyQ() {
    ParametricEqInteractionTester w;
    w.setEditorPresentationEnabled(true);
    w.resize(800, 400);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setSelectedIndex(4);
    const auto before = w.points().at(4);
    const QRect plot = w.computePlotRect();
    const QPoint handle(qRound(w.xFromFreq(plot, before.frequencyHz + 4000.0 / 24.0)),
                        qRound(w.yFromDb(plot, before.gainDb)) + 22);
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, handle);
    QTest::mouseMove(&w, handle + QPoint(25, 0));
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, handle + QPoint(25, 0));
    QCOMPARE(w.points().at(4).frequencyHz, before.frequencyHz);
    QCOMPARE(w.points().at(4).gainDb, before.gainDb);
    QVERIFY(w.points().at(4).q < before.q);
}

void TestParametricEqInteraction::widthHandleAtZeroAndNegativeGain_data() {
    QTest::addColumn<int>("count");
    QTest::addColumn<bool>("log");
    QTest::addColumn<double>("gain");
    QTest::addColumn<double>("q");
    for (int count : {5, 10, 18}) {
        for (bool log : {false, true}) {
            for (double gain : {0.0, -8.0}) {
                for (double q : {0.2, 20.0}) {
                    QTest::newRow(qPrintable(QString("%1-%2-%3-%4").arg(count).arg(log).arg(gain).arg(q))) << count << log << gain << q;
                }
            }
        }
    }
}

void TestParametricEqInteraction::widthHandleAtZeroAndNegativeGain() {
    QFETCH(int, count); QFETCH(bool, log); QFETCH(double, gain); QFETCH(double, q);
    ParametricEqInteractionTester w;
    w.setEditorPresentationEnabled(true);
    w.setBandCount(count);
    w.setLogScale(log);
    w.setQMax(20);
    w.resize(800, 400); w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    const int index = count/2;
    QVERIFY(w.setPointData(index, w.points().at(index).frequencyHz, gain, q));
    w.setSelectedIndex(index);
    const auto before = w.points().at(index);
    const QRect plot = w.computePlotRect();
    const QPoint handle = w.widthHandlePosition(plot, 1);
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, handle);
    QVERIFY(w.isDraggingNow());
    const QPoint moved = handle + QPoint(q == 0.2 ? -30 : 30, 0);
    QTest::mouseMove(&w, moved);
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);
    QCOMPARE(w.points().at(index).frequencyHz, before.frequencyHz);
    QCOMPARE(w.points().at(index).gainDb, before.gainDb);
    QVERIFY(w.points().at(index).q != before.q);
    QVERIFY(!w.isDraggingNow());
}

void TestParametricEqInteraction::clippedHandleDoesNotMutateQ() {
    for (int index : {0, 9}) {
        ParametricEqInteractionTester w;
        w.setEditorPresentationEnabled(true); w.resize(800, 400); w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setPointData(index, w.points().at(index).frequencyHz, 0, 0.2);
        w.setSelectedIndex(index);
        const QByteArray before = w.saveEditState();
        QSignalSpy committed(&w, &NereusSDR::ParametricEqWidget::pointsChanged);
        QSignalSpy started(&w, &NereusSDR::ParametricEqWidget::editStarted);
        const QPoint handle = w.widthHandlePosition(w.computePlotRect(), index == 0 ? -1 : 1);
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, handle);
        QCOMPARE(w.saveEditState(), before);
        QCOMPARE(committed.count(), 0);
        QCOMPARE(started.count(), 0);
    }
}

void TestParametricEqInteraction::clippedWidthDragIsGradualAndReturnsExactly() {
    for (bool log : {false, true}) {
        for (int index : {0, 9}) {
            ParametricEqInteractionTester w;
            w.setEditorPresentationEnabled(true);
            w.setLogScale(log);
            w.resize(800, 400);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            QVERIFY(w.setPointData(index, w.points().at(index).frequencyHz, 0.0, 0.2));
            w.setSelectedIndex(index);
            const QByteArray before = w.saveEditState();
            const QPoint handle = w.widthHandlePosition(w.computePlotRect(), index == 0 ? -1 : 1);
            const QPoint inward = handle + QPoint(index == 0 ? 1 : -1, 0);
            QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, handle);
            QTest::mouseMove(&w, inward);
            QVERIFY(w.points().at(index).q >= 0.2);
            QVERIFY(w.points().at(index).q < 0.21);
            QTest::mouseMove(&w, handle);
            QCOMPARE(w.saveEditState(), before);
            QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, handle);
            QCOMPARE(w.saveEditState(), before);
        }
    }
}

void TestParametricEqInteraction::widthDisabledWithoutQ() {
    ParametricEqInteractionTester w;
    w.setEditorPresentationEnabled(true); w.resize(800, 400); w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setSelectedIndex(4);
    const QPoint handle = w.widthHandlePosition(w.computePlotRect(), 1);
    w.setParametricEq(false);
    const QByteArray before = w.saveEditState();
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, handle);
    QVERIFY(!w.isDraggingNow());
    QTest::mouseMove(&w, handle + QPoint(30, 0));
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, handle);
    QCOMPARE(w.saveEditState(), before);
}

void TestParametricEqInteraction::gestureSignalsBracketFinalCommit() {
    ParametricEqInteractionTester w;
    w.setEditorPresentationEnabled(true); w.resize(800, 400); w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QStringList order;
    QByteArray atStart;
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::editStarted, [&] { order << "start"; atStart = w.saveEditState(); });
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::pointsChanged, [&](bool dragging) { order << (dragging ? "move" : "commit"); });
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::pointDataChanged, [&](int, int, double, double, double, bool dragging) { if (!dragging) { order << "data"; } });
    QObject::connect(&w, &NereusSDR::ParametricEqWidget::editFinished, [&] { order << "finish"; });
    w.setSelectedIndex(4);
    const QByteArray before = w.saveEditState();
    const QPoint handle = w.widthHandlePosition(w.computePlotRect(), 1);
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, handle);
    QTest::mouseMove(&w, handle + QPoint(30, 0));
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, handle + QPoint(30, 0));
    QCOMPARE(atStart, before);
    QCOMPARE(order, (QStringList{"start", "move", "commit", "data", "finish"}));
    order.clear();
    sendWheel(&w, handle, 120, Qt::ShiftModifier);
    QCOMPARE(order, (QStringList{"start", "commit", "data", "finish"}));
    order.clear();
    const QPoint dot(qRound(w.xFromFreq(w.computePlotRect(), w.points().at(4).frequencyHz)), qRound(w.yFromDb(w.computePlotRect(), w.points().at(4).gainDb)));
    const QByteArray after = w.saveEditState();
    QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, dot);
    QCOMPARE(w.saveEditState(), after);
    QVERIFY(order.isEmpty());
}

void TestParametricEqInteraction::crossingRetainsBandId() {
    ParametricEqInteractionTester w;
    w.setEditorPresentationEnabled(true); w.setBandCount(5); w.resize(800, 400); w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    const QRect plot = w.computePlotRect();
    const auto point = w.points().at(1);
    const QPoint dot(qRound(w.xFromFreq(plot, point.frequencyHz)), qRound(w.yFromDb(plot, point.gainDb)));
    const QPoint moved(qRound(w.xFromFreq(plot, w.points().at(3).frequencyHz + 100)), dot.y()-10);
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, dot);
    QTest::mouseMove(&w, moved);
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, moved);
    const int index = w.getIndexFromBandId(point.bandId);
    QVERIFY(index > 1);
    QCOMPARE(w.points().at(index).bandColor, point.bandColor);
    QCOMPARE(w.points().at(w.selectedIndex()).bandId, point.bandId);
}

void TestParametricEqInteraction::externalLoadCancelsDrag() {
    ParametricEqInteractionTester w;
    w.setEditorPresentationEnabled(true); w.resize(800, 400); w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    const QString authoritative = w.saveToJson();
    const QRect plot = w.computePlotRect();
    const QPoint dot(qRound(w.xFromFreq(plot, w.points().at(4).frequencyHz)), qRound(w.yFromDb(plot, 0)));
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, dot);
    QTest::mouseMove(&w, dot + QPoint(20, 20));
    QVERIFY(w.loadFromJson(authoritative));
    QVERIFY(!w.isDraggingNow());
    const QByteArray loaded = w.saveEditState();
    QSignalSpy commit(&w, &NereusSDR::ParametricEqWidget::pointsChanged);
    QSignalSpy finished(&w, &NereusSDR::ParametricEqWidget::editFinished);
    QTest::mouseMove(&w, dot + QPoint(50, 30));
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, dot);
    QCOMPARE(w.saveEditState(), loaded);
    QCOMPARE(commit.count(), 0);
    QCOMPARE(finished.count(), 0);
}

void TestParametricEqInteraction::runtimeStateRestoresExactValuesAndIds() {
    ParametricEqInteractionTester w;
    w.setBandCount(5);
    const int id = w.points().at(1).bandId;
    QVERIFY(w.setPointData(1, 3333.123456789, -1.23456789, 1.23456789));
    w.setGlobalGainDb(0.123456789);
    const int index = w.getIndexFromBandId(id);
    QVERIFY(index > 1);
    w.pointsMut()[index].bandColor = QColor(12, 34, 56, 78);
    const QColor color = w.points().at(index).bandColor;
    const QByteArray state = w.saveEditState();
    w.setSelectedIndex(index);
    QCOMPARE(w.saveEditState(), state);
    w.drawBarChartData(QVector<double>{1,2,3,4,5});
    QCOMPARE(w.saveEditState(), state);
    w.setBandCount(18);
    QVERIFY(w.restoreEditState(state));
    QCOMPARE(w.saveEditState(), state);
    QCOMPARE(w.bandCount(), 5);
    const auto restored = w.points().at(w.getIndexFromBandId(id));
    QCOMPARE(restored.frequencyHz, 3333.123456789);
    QCOMPARE(restored.gainDb, -1.23456789);
    QCOMPARE(restored.q, 1.23456789);
    QCOMPARE(restored.bandColor, color);
    QCOMPARE(w.globalGainDb(), 0.123456789);
    QVector<QByteArray> invalidStates;
    QByteArray badVersion = state;
    badVersion[5] = 2;
    invalidStates.append(badVersion);
    w.pointsMut()[2].bandId = w.points().at(0).bandId;
    invalidStates.append(w.saveEditState());
    QVERIFY(w.restoreEditState(state));
    w.pointsMut()[2].q = std::numeric_limits<double>::quiet_NaN();
    invalidStates.append(w.saveEditState());
    QVERIFY(w.restoreEditState(state));
    for (const QByteArray& invalid : invalidStates) {
        QVERIFY(!w.restoreEditState(invalid));
        QCOMPARE(w.saveEditState(), state);
    }
    for (const QByteArray corrupt : {state.left(state.size()-1), state + QByteArray("extra"), QByteArray("invalid")}) {
        QVERIFY(!w.restoreEditState(corrupt));
        QCOMPARE(w.saveEditState(), state);
    }
}

QTEST_MAIN(TestParametricEqInteraction)
#include "tst_parametric_eq_widget_interaction.moc"
