// no-port-check: NereusSDR-original. Test harness.
// Modification history (NereusSDR):
//   2026-11-05: a real window and Core session with a shown panadapter and
//               display, audio and speaker on test timers. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
#pragma once

#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVector>

#include <cmath>

#include "core/session/media/DaemonMediaController.h"
#include "RemoteAudioSessionHarness.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/SpectrumWidget.h"

namespace NereusSDR::Test {

// Nothing runs until start(); the pan is added by addPan(), so a test can add
// it after the session started.
struct DisplayedSessionHarness {
    RemoteAudioSessionHarness h;
    PanadapterStack stack;
    RemoteMediaController remoteMedia{&h.client, &h.remote, &stack};
    DaemonMediaController daemonMedia{&h.server, &h.station};
    QSignalSpy frames{&remoteMedia, &RemoteMediaController::displayFrameReceived};
    QSignalSpy recovery{&remoteMedia, &RemoteMediaController::recoveryRequested};
    QTimer source;
    QTimer speaker;
    QTimer display;
    QVector<float> iq = QVector<float>(2048);

    DisplayedSessionHarness()
    {
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * static_cast<float>(std::cos(double(i) * 0.17));
            iq[i + 1] = 0.01f * static_cast<float>(std::sin(double(i) * 0.17));
        }
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        QObject::connect(&source, &QTimer::timeout, &source, [this] { h.feedMixedTone(); });
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        QObject::connect(&speaker, &QTimer::timeout, &speaker,
                         [this] { h.remoteBus->render(480); });
        display.setInterval(20);
        QObject::connect(&display, &QTimer::timeout, &display, [this] {
            const int stream = h.station.sliceById(h.sliceA)->streamIndex();
            QMetaObject::invokeMethod(&h.station, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        });
    }

    bool addPan()
    {
        const int stream = h.station.sliceById(h.sliceA)->streamIndex();
        if (stream < 0) { return false; }
        auto* pan = stack.addPanadapter(QStringLiteral("displayed"));
        pan->setActiveSliceIndex(h.sliceA);
        pan->spectrumWidget()->setDisplayWindowPreservingHistory(
            h.station.streamCentreHz(stream), 48000);
        stack.resize(600, 400);
        stack.show();
        return QTest::qWaitForWindowExposed(&stack);
    }

    void stopAll()
    {
        source.stop();
        speaker.stop();
        display.stop();
    }
};

} // namespace NereusSDR::Test
