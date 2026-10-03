// =================================================================
// tests/tst_wsjtx_spot_settings.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It checks that the WSJT-X
// spot colour and lifetime saved by the Spot Hub reach the spots
// RadioModel adds; the classification it exercises cites AetherSDR in
// RadioModel.cpp.
//
// R3 controls that work, Task 1 (R-R3-21, R-R3-17). Before this change the
// Spot Hub saved four WSJT-X colours (WsjtxColorCQ / POTA / CallingMe /
// Default) that nothing read, while RadioModel read a WsjtxSpotColor that
// nothing wrote; and the Spot Life slider saved WsjtxSpotLifetime while
// RadioModel read WsjtxSpotLifetimeSec. Both settings did nothing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 controls that work, Task 1.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QColor>
#include <QSlider>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/WsjtxClient.h"
#include "gui/SpotHubDialog.h"
#include "models/RadioModel.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"

using namespace NereusSDR;

namespace {

DxSpot wsjtxSpot(const QString& call, const QString& message, double freqMhz)
{
    DxSpot spot;
    spot.dxCall = call;
    spot.freqMhz = freqMhz;
    spot.spotterCall = QStringLiteral("WSJT-X");
    spot.comment = message;
    spot.source = QStringLiteral("WSJT-X");
    spot.snr = -10;
    return spot;
}

const SpotData* spotFor(const SpotModel& model, const QString& call)
{
    for (auto it = model.spots().constBegin(); it != model.spots().constEnd(); ++it) {
        if (it.value().callsign == call) {
            return &it.value();
        }
    }
    return nullptr;
}

} // namespace

class TstWsjtxSpotSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // Each WSJT-X decode takes the colour the Spot Hub saved for its kind:
    // calling me, then CQ POTA, then CQ, then the default.
    void spotColourFollowsTheSpotHubSwatches()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("WsjtxColorCQ"),        QStringLiteral("#112233"));
        s.setValue(QStringLiteral("WsjtxColorPOTA"),      QStringLiteral("#445566"));
        s.setValue(QStringLiteral("WsjtxColorCallingMe"), QStringLiteral("#778899"));
        s.setValue(QStringLiteral("WsjtxColorDefault"),   QStringLiteral("#aabbcc"));
        s.setValue(QStringLiteral("DxClusterCallsign"),   QStringLiteral("KG4VCF"));

        RadioModel model;
        SpotModel* spots = model.spotModel();
        QVERIFY(spots != nullptr);

        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("W1AW"),
                                            QStringLiteral("CQ W1AW FN31"), 14.0741));
        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("K1ABC"),
                                            QStringLiteral("CQ POTA K1ABC FN42"), 14.0752));
        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("JA1XYZ"),
                                            QStringLiteral("KG4VCF JA1XYZ PM95"), 14.0763));
        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("DL1AA"),
                                            QStringLiteral("DL1AA G4BBB RR73"), 14.0774));

        const SpotData* cq = spotFor(*spots, QStringLiteral("W1AW"));
        const SpotData* pota = spotFor(*spots, QStringLiteral("K1ABC"));
        const SpotData* me = spotFor(*spots, QStringLiteral("JA1XYZ"));
        const SpotData* other = spotFor(*spots, QStringLiteral("DL1AA"));
        QVERIFY(cq && pota && me && other);
        QCOMPARE(QColor(cq->color), QColor(QStringLiteral("#112233")));
        QCOMPARE(QColor(pota->color), QColor(QStringLiteral("#445566")));
        QCOMPARE(QColor(me->color), QColor(QStringLiteral("#778899")));
        QCOMPARE(QColor(other->color), QColor(QStringLiteral("#aabbcc")));
    }

    // Unset swatches keep the Spot Hub's own defaults.
    void spotColourDefaultsMatchTheSpotHub()
    {
        RadioModel model;
        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("W1AW"),
                                            QStringLiteral("CQ W1AW FN31"), 14.0741));
        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("DL1AA"),
                                            QStringLiteral("DL1AA G4BBB RR73"), 14.0774));
        QCOMPARE(QColor(spotFor(*model.spotModel(), QStringLiteral("W1AW"))->color),
                 QColor(QStringLiteral("#00FF00")));
        QCOMPARE(QColor(spotFor(*model.spotModel(), QStringLiteral("DL1AA"))->color),
                 QColor(QStringLiteral("#FFFFFF")));
    }

    // The Spot Life slider's value is the lifetime a WSJT-X spot gets.
    void spotLifeSliderSetsTheSpotLifetime()
    {
        QScopedPointer<SpotHubDialog> dlg(new SpotHubDialog(
            new DxClusterClient, new DxClusterClient, new WsjtxClient,
            new SpotCollectorClient, new PotaClient, new FreeDVReporterClient,
            new PskReporterClient, new SpotModel, new SpotTableModel,
            new DxccColorProvider, nullptr));
        auto* slider = dlg->findChild<QSlider*>(QStringLiteral("wsjtxLifeSlider"));
        QVERIFY(slider != nullptr);
        slider->setValue(45);

        RadioModel model;
        emit model.wsjtx()->spotReceived(wsjtxSpot(QStringLiteral("W1AW"),
                                            QStringLiteral("CQ W1AW FN31"), 14.0741));
        const SpotData* spot = spotFor(*model.spotModel(), QStringLiteral("W1AW"));
        QVERIFY(spot != nullptr);
        QCOMPARE(spot->lifetimeSeconds, 45);
    }

    // The slider shows the saved lifetime, read from the name RadioModel
    // reads.
    void spotLifeSliderShowsTheSavedLifetime()
    {
        AppSettings::instance().setValue(QStringLiteral("WsjtxSpotLifetimeSec"), 200);
        QScopedPointer<SpotHubDialog> dlg(new SpotHubDialog(
            new DxClusterClient, new DxClusterClient, new WsjtxClient,
            new SpotCollectorClient, new PotaClient, new FreeDVReporterClient,
            new PskReporterClient, new SpotModel, new SpotTableModel,
            new DxccColorProvider, nullptr));
        auto* slider = dlg->findChild<QSlider*>(QStringLiteral("wsjtxLifeSlider"));
        QVERIFY(slider != nullptr);
        QCOMPARE(slider->value(), 200);
    }

    // A lifetime saved under the old name before this change is kept: the
    // startup migration reads it once and writes the name everything reads.
    void savedLifetimeUnderTheOldNameIsMigrated()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings s(dir.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("WsjtxSpotLifetime"), 90);

        AppSettings::migrateRenamedKeys(s);
        QCOMPARE(s.value(QStringLiteral("WsjtxSpotLifetimeSec")).toInt(), 90);
        QVERIFY(!s.contains(QStringLiteral("WsjtxSpotLifetime")));

        // Idempotent, and a value already saved under the new name wins.
        s.setValue(QStringLiteral("WsjtxSpotLifetime"), 30);
        s.setValue(QStringLiteral("WsjtxSpotLifetimeSec"), 250);
        AppSettings::migrateRenamedKeys(s);
        QCOMPARE(s.value(QStringLiteral("WsjtxSpotLifetimeSec")).toInt(), 250);
        QVERIFY(!s.contains(QStringLiteral("WsjtxSpotLifetime")));
    }
};

QTEST_MAIN(TstWsjtxSpotSettings)
#include "tst_wsjtx_spot_settings.moc"
