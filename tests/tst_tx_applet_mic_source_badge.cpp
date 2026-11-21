// =================================================================
// tests/tst_tx_applet_mic_source_badge.cpp  (Longpath)
// =================================================================
//
// Longpath-original test file. No Thetis port at this layer.
//
// Verifies the TxApplet mic-source badge text for all three MicSource
// values: Pc -> "PC mic", Radio -> "Radio mic", Vax -> "VAX". Covers
// both the live micSourceChanged path and the syncFromModel path.
// =================================================================
//
// Modification history (Longpath):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest/QtTest>
#include <QLabel>

#include <atomic>
#include <memory>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

namespace {
QLabel* findBadge(TxApplet* applet)
{
    const auto labels = applet->findChildren<QLabel*>();
    for (auto* l : labels) {
        if (l->accessibleName() == QStringLiteral("Mic source indicator")) {
            return l;
        }
    }
    return nullptr;
}

/// Aufnahmebus, dessen letzter Rueckruf `ns` her ist (2026-11-20).
class GestellterBus : public IAudioBus {
public:
    explicit GestellterBus(std::shared_ptr<std::atomic<qint64>> ns) : m_ns(std::move(ns)) {}
    bool open(const AudioFormat&) override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    qint64 push(const char*, qint64 b) override { return b; }
    qint64 pull(char*, qint64) override { return 0; }
    float rxLevel() const override { return 0.0F; }
    float txLevel() const override { return 0.0F; }
    QString backendName() const override { return QStringLiteral("GestellterBus"); }
    AudioFormat negotiatedFormat() const override { return {}; }
    qint64 nsSinceLastCallback() const override { return m_ns->load(); }
private:
    std::shared_ptr<std::atomic<qint64>> m_ns;
};
}

class TstTxAppletMicSourceBadge : public QObject {
    Q_OBJECT

private slots:

    void liveChange_Pc()
    {
        RadioModel model;
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        model.transmitModel().setMicSource(MicSource::Radio);
        QCOMPARE(badge->text(), QStringLiteral("Radio mic"));
        model.transmitModel().setMicSource(MicSource::Pc);
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));
    }

    void liveChange_Vax()
    {
        RadioModel model;
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        model.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
    }

    void syncFromModel_Vax()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Vax);

        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);
        applet.syncFromModel();
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
    }

    // 2026-11-20 (Entwurf A): das PC-Mikrofon liefert nicht mehr — die
    // Fusszeile sagt es, nennt im Tooltip, was zu tun ist, und kehrt zu
    // "PC mic" zurueck, sobald es wieder liefert. Bei Quelle "Radio mic"
    // schweigt sie, auch wenn der PC-Eingang tot ist.
    void pcMikrofonAusfall()
    {
        RadioModel model;
        AudioEngine* ae = model.audioEngine();
        QVERIFY(ae != nullptr);
        auto ns = std::make_shared<std::atomic<qint64>>(qint64(2) * 1000000);
        ae->setTxInputBusForTest(std::make_unique<GestellterBus>(ns));
        ae->setTonLaeuftFuerTest(true);
        model.transmitModel().setMicSource(MicSource::Pc);
        ae->onMicSourceChanged(true);

        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        qint64 t = 0;
        for (; t < 2000; t += 250) { ae->pruefePcMikrofon(t); }
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));

        ns->store(qint64(5000) * 1000000);   // ausgesteckt
        ae->pruefePcMikrofon(t); t += 250;
        QVERIFY2(badge->text().contains(QStringLiteral("no signal")),
                 qPrintable(badge->text()));
        QVERIFY(badge->toolTip().contains(QStringLiteral("stopped delivering audio")));
        QVERIFY(badge->toolTip().contains(QStringLiteral("restart Longpath")));

        // Quelle auf die Maschine: keine Warnung mehr, obwohl der Bus tot ist.
        model.transmitModel().setMicSource(MicSource::Radio);
        ae->onMicSourceChanged(false);
        ae->pruefePcMikrofon(t); t += 250;
        QCOMPARE(badge->text(), QStringLiteral("Radio mic"));

        // Zurueck auf PC, und das Mikrofon liefert wieder.
        model.transmitModel().setMicSource(MicSource::Pc);
        ae->onMicSourceChanged(true);
        ns->store(qint64(3) * 1000000);
        for (int i = 0; i < 4; ++i, t += 250) { ae->pruefePcMikrofon(t); }
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));
    }
};

QTEST_MAIN(TstTxAppletMicSourceBadge)
#include "tst_tx_applet_mic_source_badge.moc"
