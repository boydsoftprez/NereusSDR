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
#include <QScopeGuard>

#include "gui/applets/TxApplet.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/styles/AppTheme.h"
#include "core/session/StationClient.h"
#include <QRadioButton>
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

    void remoteRadioWithoutNegotiatedCommandIsDisabledAndCannotChangeInput()
    {
        const QPalette previousPalette = qApp->palette();
        const QString previousQss = qApp->styleSheet();
        const auto restoreTheme = qScopeGuard([previousPalette, previousQss]() {
            qApp->setPalette(previousPalette);
            qApp->setStyleSheet(previousQss);
        });
        if (!qEnvironmentVariable("NEREUS_RADIO_CAPTURE_DIR").isEmpty()) {
            applyDarkPalette(*qApp);
            applyAppBaselineQss(*qApp);
        }
        RadioModel model(RadioModel::Role::Remote);
        model.setBoardForTest(HPSDRHW::Hermes);
        StationClient client(&model, nullptr);
        model.attachStation(&client);
        model.transmitModel().setMicSourceLocked(false);
        model.transmitModel().setMicSource(MicSource::Pc);
        AudioTxInputPage page(&model);
        QVERIFY(!page.radioMicButton()->isEnabled());
        QVERIFY(page.radioMicButton()->toolTip().contains(QStringLiteral("Core")));
        page.radioMicButton()->click();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
        if (const QString captures = qEnvironmentVariable("NEREUS_RADIO_CAPTURE_DIR"); !captures.isEmpty()) {
            page.resize(640, 760);
            page.show();
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(captures + QStringLiteral("/radio-microphone-legacy-core.png")));
        }
    }

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
