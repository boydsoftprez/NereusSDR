// no-port-check: NereusSDR-original test helper.
// Modification history (NereusSDR):
//   2026-10-26 — Skip GPU frame cases where QRhi is unavailable by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
// Some platforms cannot render QRhi frames at all: Qt's `offscreen` platform
// reports no RhiBasedRendering capability, and QRhiWidget then logs "QRhi is
// not supported on this platform" and never submits a frame. A case that needs
// GPU frames starts with NEREUS_SKIP_WITHOUT_QRHI(), which skips it there with
// a reason and does nothing where QRhi works (CI runs these under xvfb). The
// skip covers only that missing capability; Hidden and Stalled stay failures.
//
// See docs/development/fast-test-loop.md, "Test windows".
#pragma once

#include <QSignalSpy>
#include <QWidget>
#include <QWindow>
#include <QtTest>

#ifdef NEREUS_GPU_SPECTRUM
#include <QByteArray>
#include <QGuiApplication>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#endif

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

#ifdef NEREUS_GPU_SPECTRUM
/// True when this process's platform plugin can render QRhi frames. It asks
/// the same question QRhiWidget's constructor asks before it turns on QRhi
/// flushing (QPlatformIntegration::RhiBasedRendering, Qt 6.11); without that
/// capability QRhiWidget logs "QRhi is not supported on this platform" and no
/// frame is ever submitted. QRhi::probe() answers a different question:
/// whether a graphics API (Metal, Vulkan, OpenGL) can start in this process,
/// not whether this platform's widgets render through QRhi.
inline bool canRenderRhiFrames()
{
    const QPlatformIntegration* integration = QGuiApplicationPrivate::platformIntegration();
    return integration != nullptr
        && integration->hasCapability(QPlatformIntegration::RhiBasedRendering);
}

/// The reason a case reports when it is skipped for want of QRhi.
inline QByteArray noRhiSkipReason()
{
    return QByteArrayLiteral("QRhi is unavailable on the '")
        + QGuiApplication::platformName().toUtf8()
        + QByteArrayLiteral("' platform; this case needs GPU frames and runs in CI under xvfb");
}
#endif

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

// Skips the calling case, with a reason naming the platform, when this process
// cannot render QRhi frames (Qt's offscreen platform). Does nothing where QRhi
// works, and nothing in a build without GPU rendering, whose cases grab widget
// images instead of waiting for GPU frames. Like QSKIP it returns from the
// calling function, so call it first, before any setup. It never excuses a
// Hidden or Stalled wait: NEREUS_VERIFY_FRAME still fails those.
#ifdef NEREUS_GPU_SPECTRUM
#define NEREUS_SKIP_WITHOUT_QRHI()                                                          \
    do {                                                                                    \
        if (!::NereusSDR::NativeWindowFrames::canRenderRhiFrames()) {                       \
            const QByteArray nereusNoRhiReason =                                            \
                ::NereusSDR::NativeWindowFrames::noRhiSkipReason();                         \
            QSKIP(nereusNoRhiReason.constData());                                           \
        }                                                                                   \
    } while (false)
#else
#define NEREUS_SKIP_WITHOUT_QRHI() \
    do {                           \
    } while (false)
#endif
