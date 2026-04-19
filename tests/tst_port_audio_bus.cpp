#include <QtTest/QtTest>
#include "core/audio/PortAudioBus.h"
#include <portaudio.h>

using namespace NereusSDR;

class TstPortAudioBus : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        Pa_Initialize();
    }
    void cleanupTestCase() {
        Pa_Terminate();
    }

    void constructsClosed() {
        PortAudioBus bus;
        QVERIFY(!bus.isOpen());
    }

    void openSucceedsOnDefaultDevice() {
        PortAudioBus bus;
        AudioFormat f;
        QVERIFY2(bus.open(f), qPrintable(bus.errorString()));
        QVERIFY(bus.isOpen());
        bus.close();
        QVERIFY(!bus.isOpen());
    }

    void negotiatedFormatReflectsDevice() {
        PortAudioBus bus;
        AudioFormat f;
        f.sampleRate = 48000;
        f.channels = 2;
        bus.open(f);
        QVERIFY(bus.negotiatedFormat().sampleRate > 0);
        bus.close();
    }

    void backendNameIdentifiesAPI() {
        PortAudioBus bus;
        bus.open(AudioFormat{});
        const QString n = bus.backendName();
        QVERIFY(!n.isEmpty());
        bus.close();
    }
};

QTEST_APPLESS_MAIN(TstPortAudioBus)
#include "tst_port_audio_bus.moc"
