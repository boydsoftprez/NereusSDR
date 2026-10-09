// no-port-check: NereusSDR-original test helper.
//
// NativeWindowFrames: waits for a QRhiWidget in a NATIVE_WINDOW test to
// submit GPU frames, and tells a hidden window apart from a stalled one.
//
// macOS draws nothing for a window it reports occluded. Qt's Cocoa plugin
// turns the occlusion notice into an empty expose
// (QCocoaWindow::windowDidChangeOcclusionState, qcocoawindow.mm, Qt 6.11),
// QWidgetWindow then clears Qt::WA_Mapped, the repaint manager discards every
// sync, and no frameSubmitted arrives however long the test waits. A test
// window gets hidden that way when:
//   - the app that was frontmost takes focus back about a second after the
//     test process launches and covers the new window (seen on 2026-10-08 in
//     about two of three runs while the Claude desktop app was frontmost);
//   - the operator switches to a full-screen app;
//   - another native test's window covers it in a parallel ctest run.
// Waiting for a frame then failed as a GPU "stall" although nothing in the
// product stalled: tst_spectrum_gpu_buffer_growth failed whichever case ran
// first, 2D included, and tst_meter_bar_face failed whenever Qt logged its
// window "changed to occluded".
//
// The helpers wait for the frame or for the window to be hidden, whichever
// comes first. A hidden window is raised (ordered front without activating
// the app, so it takes no keyboard focus) and the frame is asked for again.
// A window that is still hidden after kMaxRaises raises is reported Hidden,
// a failure that says so: the case checked nothing, and a skip would count
// as a pass. A window that stays exposed and still submits no frame within
// the timeout is Stalled, the product failure these waits exist to catch;
// the timeouts are the callers' own and unchanged.
//
// See docs/development/fast-test-loop.md, "Test windows".
#pragma once

#include <QSignalSpy>
#include <QWidget>
#include <QWindow>
#include <QtTest>

namespace NereusSDR::NativeWindowFrames {

enum class FrameWait { Submitted, Hidden, Stalled };

inline constexpr int kMaxRaises = 3;
inline constexpr int kReexposeTimeoutMs = 5000;
inline constexpr char kHiddenFailure[] =
    "macOS kept the test window hidden (occluded) after raising it, and it "
    "draws nothing while hidden, so no GPU frame could be checked; not a "
    "product stall. Rerun with the window unobstructed.";
inline constexpr char kStalledFailure[] =
    "no GPU frame arrived while the test window was exposed";

/// True when the top-level native window holding `widget` is not exposed.
/// Occlusion reaches only the top-level window, so a child's own handle can
/// still read as exposed while nothing draws.
inline bool isHidden(const QWidget& widget)
{
    const QWidget* top = widget.window();
    const QWindow* window = top ? top->windowHandle() : nullptr;
    return !window || !window->isExposed();
}

/// Waits until `frames` has recorded at least `count` signals. A hidden
/// window is raised and redrawn, and the wait resumes with a fresh timeout.
/// `raises`, when given, receives how many times the window was raised.
inline FrameWait waitForFrames(QWidget& widget, const QSignalSpy& frames,
                               qsizetype count, int timeoutMs,
                               int* raises = nullptr)
{
    int raised = 0;
    const auto finish = [&](FrameWait outcome) {
        if (raises) {
            *raises = raised;
        }
        return outcome;
    };
    for (;;) {
        bool hidden = false;
        // The outcome is read from the spy and `hidden` below.
        static_cast<void>(QTest::qWaitFor([&] {
            if (frames.count() >= count) {
                return true;
            }
            hidden = isHidden(widget);
            return hidden;
        }, timeoutMs));
        if (frames.count() >= count) {
            return finish(FrameWait::Submitted);
        }
        if (!hidden) {
            return finish(FrameWait::Stalled);
        }
        if (raised == kMaxRaises) {
            return finish(FrameWait::Hidden);
        }
        ++raised;
        QWidget* top = widget.window();
        top->raise();
        if (!QTest::qWaitForWindowExposed(top, kReexposeTimeoutMs)) {
            return finish(FrameWait::Hidden);
        }
        widget.update();
    }
}

/// Asks `widget` for one new frame and waits for it, as waitForFrames().
inline FrameWait requestFrame(QWidget& widget, const QSignalSpy& frames,
                              int timeoutMs, int* raises = nullptr)
{
    const qsizetype before = frames.count();
    widget.update();
    return waitForFrames(widget, frames, before + 1, timeoutMs, raises);
}

} // namespace NereusSDR::NativeWindowFrames

// QVERIFY-style check of a FrameWait: passes on Submitted and fails with the
// matching message otherwise. Like QVERIFY it returns from the calling
// function.
#define NEREUS_VERIFY_FRAME(frameWait)                                                      \
    do {                                                                                    \
        const ::NereusSDR::NativeWindowFrames::FrameWait nereusFrameWait = (frameWait);     \
        QVERIFY2(nereusFrameWait == ::NereusSDR::NativeWindowFrames::FrameWait::Submitted,  \
                 nereusFrameWait == ::NereusSDR::NativeWindowFrames::FrameWait::Hidden      \
                     ? ::NereusSDR::NativeWindowFrames::kHiddenFailure                      \
                     : ::NereusSDR::NativeWindowFrames::kStalledFailure);                   \
    } while (false)
