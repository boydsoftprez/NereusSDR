// =================================================================
// tests/tst_tx_applet_mic_source_badge.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the TxApplet mic-source badge text for all three MicSource
// values: Pc -> "PC mic", Radio -> "Radio mic", Vax -> "VAX". Covers
// both the live micSourceChanged path and the syncFromModel path.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QLabel>

#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

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
};

QTEST_MAIN(TstTxAppletMicSourceBadge)
#include "tst_tx_applet_mic_source_badge.moc"
