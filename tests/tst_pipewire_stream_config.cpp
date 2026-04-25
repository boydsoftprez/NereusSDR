// =================================================================
// tests/tst_pipewire_stream_config.cpp  (NereusSDR)
// Author: J.J. Boyd (KG4VCF), AI-assisted via Claude Code. 2026-04-23.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE

#include <QtTest/QtTest>
#include <pipewire/pipewire.h>
#include <pipewire/keys.h>
#include "core/audio/PipeWireStream.h"

using namespace NereusSDR;

class TestPipeWireStreamConfig : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { pw_init(nullptr, nullptr); }
    void cleanupTestCase() { pw_deinit(); }

    void outputVirtualSource_setsCoreKeys() {
        StreamConfig cfg;
        cfg.nodeName        = QStringLiteral("nereussdr.vax-1");
        cfg.nodeDescription = QStringLiteral("NereusSDR VAX 1");
        cfg.direction       = StreamConfig::Output;
        cfg.mediaClass      = QStringLiteral("Audio/Source");
        cfg.mediaRole       = QStringLiteral("Music");
        cfg.rate = 48000; cfg.channels = 2; cfg.quantum = 512;

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_NODE_NAME)),
                 cfg.nodeName);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_MEDIA_CLASS)),
                 cfg.mediaClass);
        // Audio/Source + OUTPUT direction = virtual source node: media.category
        // must be "Capture" (consumer's perspective, not ours). "Playback" is
        // wrong for this class and breaks ALSA-compat bridge enumeration.
        // Expectation updated to match the correct configToProperties() output.
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_MEDIA_CATEGORY)),
                 QStringLiteral("Capture"));
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_NODE_LATENCY)),
                 QStringLiteral("512/48000"));
        pw_properties_free(p);
    }

    void inputConsumer_setsCaptureCategory() {
        StreamConfig cfg;
        cfg.nodeName = QStringLiteral("nereussdr.tx-input");
        cfg.nodeDescription = QStringLiteral("NereusSDR TX input");
        cfg.direction = StreamConfig::Input;
        cfg.mediaClass = QStringLiteral("Stream/Input/Audio");
        cfg.mediaRole = QStringLiteral("Phone");

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_MEDIA_CATEGORY)),
                 QStringLiteral("Capture"));
        pw_properties_free(p);
    }

    void targetNodeName_setsTargetObject() {
        StreamConfig cfg;
        cfg.nodeName = QStringLiteral("nereussdr.rx-primary");
        cfg.nodeDescription = QStringLiteral("NereusSDR Primary");
        cfg.direction = StreamConfig::Output;
        cfg.mediaClass = QStringLiteral("Stream/Output/Audio");
        cfg.mediaRole = QStringLiteral("Music");
        cfg.targetNodeName = QStringLiteral("alsa_output.usb-headphones");

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_TARGET_OBJECT)),
                 cfg.targetNodeName);
        pw_properties_free(p);
    }
};

QTEST_MAIN(TestPipeWireStreamConfig)
#include "tst_pipewire_stream_config.moc"

#else
int main() { return 0; }   // Compiles cleanly on hosts without libpipewire.
#endif
