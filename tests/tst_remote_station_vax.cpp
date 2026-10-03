// no-port-check: NereusSDR-original. iPhone app plan Task 25 (R-IOS-18): a
// remote window's VAX applet keeps this computer's own VAX channels
// (R-R3-44) and, below them, shows the Core computer's in its "Station
// computer" section, driven by the Core's `vax` object: its slices, levels,
// mutes and device names, every control changing the Core's through the
// object, its TX row disabled with the gate's reason while this device may
// not transmit, and its meters subscribed only while the applet shows it.
// Loopback link, no radio, no audio device opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created (iPhone app plan Task 25,
//                                    R-IOS-18). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  JJ's ruling: a local window hides the
//                                    section; a remote one shows it disabled
//                                    with its reason, labels greyed, while
//                                    the Core shares no VAX. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QCoreApplication>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/StationVaxFacade.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/applets/VaxApplet.h"
#include "gui/StyleConstants.h"
#include "gui/styles/AppTheme.h"
#include "gui/widgets/MeterSlider.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:5B");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = kMac;
    info.name = QStringLiteral("Bench G2");
    info.boardType = HPSDRHW::Saturn;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A Core the desktop hosts and one remote window, handshake complete.
struct Session {
    Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
    {
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        core = makeStationRadioModel();
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    ~Session()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        client.reset();
        server.reset();
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    AudioEngine* coreAudio() const { return core->localAudioDevices(); }

    QTemporaryDir settingsDir;
    AppSettings settings;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

// The applet as MainWindow wires it in a remote window
// (refreshRemoteStationVax and the stationLevelsWantedChanged connection).
void wire(VaxApplet& applet, StationClient& client)
{
    QObject::connect(&applet, &VaxApplet::stationLevelsWantedChanged, &client,
                     [&client](bool wanted) { client.setStationVaxLevelsWanted(wanted); });
    const auto refresh = [&applet, &client]() {
        applet.setStationVax(client.stationVax(), client.stationVaxHeld());
        client.setStationVaxLevelsWanted(applet.stationLevelsWanted());
    };
    QObject::connect(&client, &StationClient::stationVaxAvailabilityChanged, &applet, refresh);
    refresh();
}

} // namespace

class TstRemoteStationVax : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("remote-station-vax-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
    }
    void cleanupTestCase()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // JJ's ruling (2026-09-30): a window that runs the radio directly can
    // never have a "Core computer" section (its own rows are the Core
    // computer's), so the section is hidden there; a remote window keeps
    // it, disabled with its reason while the Core shares no VAX.
    void aLocalWindowHidesTheSection()
    {
        RadioModel local;
        AudioEngine audio;
        VaxApplet applet(&local, &audio);
        applet.show();
        QWidget* const section = applet.stationSectionForTest();
        QVERIFY(section != nullptr);
        QVERIFY(!section->isVisibleTo(&applet));
        QVERIFY(!section->isEnabled());
        QVERIFY(!applet.stationLevelsWanted());
    }

    // The window declares vax, holds the Core's object and shows the
    // section with the Core's values, below its own rows.
    void theSectionShowsTheCoresChannels()
    {
        Session s(m_securityDir.path(), this);
        s.coreAudio()->setVaxRxGain(2, 0.4f);
        s.coreAudio()->setVaxMuted(3, true);
        s.core->sliceById(0)->setVaxChannel(2);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().vaxVersion, 1);
        QTRY_VERIFY(s.client->stationVaxHeld());

        AudioEngine ownAudio;
        VaxApplet applet(&s.window, &ownAudio);
        wire(applet, *s.client);
        QVERIFY(applet.stationSectionForTest()->isVisibleTo(&applet));
        QVERIFY(applet.stationSectionForTest()->isEnabled());
        for (QLabel* label : applet.stationSectionForTest()->findChildren<QLabel*>()) {
            QVERIFY2(label->isEnabled(), qPrintable(label->text()));
        }
        // Built disabled with the reason, the controls get their own
        // tooltips back once the Core sends its VAX.
        QCOMPARE(applet.stationMuteButtonForTest(3)->toolTip(),
                 QStringLiteral("Mute VAX channel 3 on the computer the Core runs on"));
        QVERIFY(applet.stationRxMeterForTest(1)->toolTip().isEmpty());
        QVERIFY(applet.stationSectionForTest()->toolTip().isEmpty());
        StationVax* copy = s.client->stationVax();
        QTRY_COMPARE(copy->rxGain(2), 0.4);
        QTRY_COMPARE(applet.stationRxMeterForTest(2)->gain(), 0.4f);
        QVERIFY(applet.stationMuteButtonForTest(3)->isChecked());
        QTRY_COMPARE(applet.stationTagsLabelForTest(2)->text(), QStringLiteral("Slice A"));
        QCOMPARE(applet.stationDeviceLabelForTest(1)->text(), StationVax::deviceName(1));
        // This computer's own channels are untouched (R-R3-44).
        QCOMPARE(ownAudio.vaxRxGain(2), 1.0f);
    }

    // Each control changes the Core's channel through the object, and a
    // change on the Core's computer reaches the section.
    void theControlsChangeTheCoresChannels()
    {
        Session s(m_securityDir.path(), this);
        QVERIFY(s.connect());
        QTRY_VERIFY(s.client->stationVaxHeld());
        AudioEngine ownAudio;
        VaxApplet applet(&s.window, &ownAudio);
        wire(applet, *s.client);

        applet.stationRxMeterForTest(1)->setGain(0.3f);
        emit applet.stationRxMeterForTest(1)->gainChanged(0.3f);
        QTRY_COMPARE(s.coreAudio()->vaxRxGain(1), 0.3f);
        applet.stationMuteButtonForTest(4)->setChecked(true);
        QTRY_VERIFY(s.coreAudio()->vaxMuted(4));
        // Saved on the Core under the applet's keys.
        QCOMPARE(s.settings.value(StationVax::mutedKey(4)).toString(), QStringLiteral("True"));
        QCOMPARE(ownAudio.vaxRxGain(1), 1.0f);
        QVERIFY(!ownAudio.vaxMuted(4));

        s.coreAudio()->setVaxRxGain(3, 0.6f);
        s.coreAudio()->setVaxMuted(4, false);
        QTRY_COMPARE(applet.stationRxMeterForTest(3)->gain(), 0.6f);
        QTRY_VERIFY(!applet.stationMuteButtonForTest(4)->isChecked());
    }

    // The TX row: disabled with the gate's reason while this device may not
    // transmit, and the Core refuses a write from it anyway.
    void theTxRowFollowsTransmitPermission()
    {
        Session s(m_securityDir.path(), this);
        QVERIFY(s.connect());
        QTRY_VERIFY(s.client->stationVaxHeld());
        QVERIFY(!s.client->capabilities().txPermitted);
        AudioEngine ownAudio;
        VaxApplet applet(&s.window, &ownAudio);
        wire(applet, *s.client);
        const QString reason = s.client->capabilities().txRefusalReason;
        QVERIFY(!reason.isEmpty());
        applet.setStationTransmitPermitted(false, reason);
        QVERIFY(!applet.stationTxMeterForTest()->isEnabled());
        QCOMPARE(applet.stationTxMeterForTest()->toolTip(), reason);
        QVERIFY(OperatorWording::isPlain(reason));
        // A write that got past the control is still refused by the Core.
        s.coreAudio()->setVaxTxGain(1.0f);
        QTRY_COMPARE(s.client->stationVax()->txGain(), 1.0);
        QSignalSpy refused(s.client.get(), &StationClient::propertyWriteCompleted);
        s.client->stationVax()->setTxGain(0.2);
        const auto txGainResult = [&refused]() -> const QList<QVariant>* {
            for (const QList<QVariant>& args : refused) {
                if (args.at(0).toByteArray() == QByteArrayLiteral("vax")
                    && args.at(1).toByteArray() == QByteArrayLiteral("txGain")) {
                    return &args;
                }
            }
            return nullptr;
        };
        QTRY_VERIFY(txGainResult() != nullptr);
        QVERIFY(!txGainResult()->at(3).toBool());
        QCOMPARE(s.coreAudio()->vaxTxGain(), 1.0f);
        // The refusal itself carries the Core's value: the copy and the row
        // snap back at once, without waiting for the Core to send it again.
        QCOMPARE(s.client->stationVax()->txGain(), 1.0);
        QCOMPARE(applet.stationTxMeterForTest()->gain(), 1.0f);

        applet.setStationTransmitPermitted(true, QString());
        QVERIFY(applet.stationTxMeterForTest()->isEnabled());
    }

    // The meters: subscribed only while the section is shown and the
    // applet visible, and they draw what the Core reads.
    void theMetersFollowTheAppletsVisibility()
    {
        Session s(m_securityDir.path(), this);
        s.server->setVaxLevelReaderForTest([](double* rx, double* tx) {
            rx[0] = 0.5;
            rx[1] = 0.25;
            rx[2] = 0.0;
            rx[3] = 0.75;
            *tx = 0.1;
        });
        QVERIFY(s.connect());
        QTRY_VERIFY(s.client->stationVaxHeld());
        AudioEngine ownAudio;
        VaxApplet applet(&s.window, &ownAudio);
        wire(applet, *s.client);
        QVERIFY(!applet.stationLevelsWanted());
        QTest::qWait(100);
        QVERIFY(!s.server->vaxLevelsPollingForTest());

        applet.show();
        QVERIFY(applet.stationLevelsWanted());
        QTRY_VERIFY(s.server->vaxLevelsPollingForTest());
        s.server->pollVaxLevelsForTest();
        QTRY_COMPARE(applet.stationRxMeterForTest(1)->level(), 0.5f);
        QCOMPARE(applet.stationRxMeterForTest(4)->level(), 0.75f);
        QCOMPARE(applet.stationTxMeterForTest()->level(), 0.1f);

        applet.hide();
        QVERIFY(!applet.stationLevelsWanted());
        QTRY_VERIFY(!s.server->vaxLevelsPollingForTest());
    }

    // A remote window whose Core publishes no VAX devices (nereusd): the
    // section stays in place, disabled, with the plain reason on each
    // control and its labels greyed.
    void aHeadlessCoreShowsTheSectionDisabled()
    {
        Session s(m_securityDir.path(), this);
        s.coreAudio()->setVaxOutputsAllowed(false);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().vaxVersion, 0);
        QVERIFY(!s.client->stationVaxHeld());
        AudioEngine ownAudio;
        VaxApplet applet(&s.window, &ownAudio);
        wire(applet, *s.client);
        applet.show();
        QVERIFY(applet.stationSectionForTest()->isVisibleTo(&applet));
        QVERIFY(!applet.stationSectionForTest()->isEnabled());
        QVERIFY(!applet.stationLevelsWanted());
        verifyDisabledWithReason(
            applet, QStringLiteral("The Core computer is not sharing its VAX channels."));
        QVERIFY(!QTest::currentTestFailed());
    }

    // A level slider that cannot act looks disabled: the style guide's
    // disabled trio, as the dark page style's disabled QSlider rules use it
    // (groove kDisabledBg, fill kDisabledBorder, thumb kDisabledText), and
    // no accent color anywhere. Both TX rows in a window that may not
    // transmit draw that way.
    void aDisabledLevelSliderLooksDisabled()
    {
        MeterSlider slider;
        slider.resize(200, slider.height());
        slider.setGain(0.5f);
        const auto pixel = [&slider](double at) {
            const QImage image = slider.grab().toImage();
            return image.pixelColor(static_cast<int>(image.width() * at), image.height() / 2)
                .name();
        };
        QCOMPARE(pixel(0.75), QStringLiteral("#0a0a18"));
        QCOMPARE(slider.cursor().shape(), Qt::PointingHandCursor);

        slider.setEnabled(false);
        QCOMPARE(pixel(0.75), QString::fromLatin1(Style::kDisabledBg));
        QCOMPARE(pixel(0.25), QString::fromLatin1(Style::kDisabledBorder));
        const QImage image = slider.grab().toImage();
        const QColor accent(0x00, 0xb4, 0xd8);
        bool thumbDisabled = false;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor c = image.pixelColor(x, y);
                QVERIFY2(c.rgb() != accent.rgb(), "an accent pixel on a disabled slider");
                thumbDisabled = thumbDisabled || c.name() == QLatin1String(Style::kDisabledText);
            }
        }
        QVERIFY(thumbDisabled);
        QCOMPARE(slider.cursor().shape(), Qt::ArrowCursor);

        slider.setEnabled(true);
        QCOMPARE(pixel(0.75), QStringLiteral("#0a0a18"));

        // The applet: this computer's TX row and the Core computer's.
        Session s(m_securityDir.path(), this);
        QVERIFY(s.connect());
        QTRY_VERIFY(s.client->stationVaxHeld());
        AudioEngine ownAudio;
        VaxApplet applet(&s.window, &ownAudio);
        wire(applet, *s.client);
        applet.setStationTransmitPermitted(false, s.client->capabilities().txRefusalReason);
        int disabled = 0;
        for (MeterSlider* meter : applet.findChildren<MeterSlider*>()) {
            if (!meter->isEnabled()) {
                ++disabled;
                QVERIFY(!meter->toolTip().isEmpty());
            }
        }
        QCOMPARE(disabled, 2);
    }

    // Offscreen renders for the report: NEREUS_VAX_RENDER_DIR names where.
    void rendersForTheReport()
    {
        const QString dir = qEnvironmentVariable("NEREUS_VAX_RENDER_DIR");
        if (dir.isEmpty()) {
            QSKIP("Set NEREUS_VAX_RENDER_DIR to save the renders.");
        }
        // As the app draws itself (main.cpp): Fusion and the dark palette.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        Session s(m_securityDir.path(), this);
        s.core->sliceById(0)->setVaxChannel(1);
        s.coreAudio()->setVaxRxGain(2, 0.6f);
        s.coreAudio()->setVaxMuted(3, true);
        QVERIFY(s.connect());
        QTRY_VERIFY(s.client->stationVaxHeld());
        AudioEngine ownAudio;
        VaxApplet without(&s.window, &ownAudio);
        without.resize(300, without.sizeHint().height());
        without.show();
        QVERIFY(without.grab().save(QDir(dir).filePath(QStringLiteral("vax-applet-without-station.png"))));
        VaxApplet with(&s.window, &ownAudio);
        wire(with, *s.client);
        with.setStationTransmitPermitted(false, s.client->capabilities().txRefusalReason);
        with.resize(300, with.sizeHint().height());
        with.show();
        QTRY_COMPARE(with.stationTagsLabelForTest(1)->text(), QStringLiteral("Slice A"));
        QVERIFY(with.grab().save(QDir(dir).filePath(QStringLiteral("vax-applet-with-station.png"))));
    }

private:
    // The section cannot be used: titled "Core computer", and it and every
    // control in it (title, RX rows, mutes, TX row) show `reason`, which
    // is plain and does not call the Core a station.
    static void verifyDisabledWithReason(const VaxApplet& applet, const QString& reason)
    {
        QWidget* const section = applet.stationSectionForTest();
        QVERIFY(!section->isEnabled());
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY2(OperatorWording::coreCalledStationIn(reason).isEmpty(), qPrintable(reason));
        QCOMPARE(section->toolTip(), reason);
        bool titled = false;
        for (QLabel* label : section->findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("Core computer")) {
                titled = true;
                QCOMPARE(label->toolTip(), reason);
            }
        }
        QVERIFY(titled);
        // Every label greys with the section, as the title does: the VAX
        // and TX labels, the tag labels and the device labels.
        const QString greyed =
            QStringLiteral("QLabel:disabled { color: %1; }").arg(QLatin1String(Style::kDisabledText));
        const QList<QLabel*> labels = section->findChildren<QLabel*>();
        QCOMPARE(labels.size(), 1 + 4 * 3 + 2);
        for (QLabel* label : labels) {
            QVERIFY2(!label->isEnabled(), qPrintable(label->text()));
            QVERIFY2(label->styleSheet().contains(greyed),
                     qPrintable(label->text() + QStringLiteral(": ") + label->styleSheet()));
        }
        for (int channel = 1; channel <= 4; ++channel) {
            QCOMPARE(applet.stationRxMeterForTest(channel)->toolTip(), reason);
            QCOMPARE(applet.stationMuteButtonForTest(channel)->toolTip(), reason);
            QVERIFY(!applet.stationRxMeterForTest(channel)->isEnabled());
            QVERIFY(!applet.stationMuteButtonForTest(channel)->isEnabled());
        }
        QCOMPARE(applet.stationTxMeterForTest()->toolTip(), reason);
        QVERIFY(!applet.stationTxMeterForTest()->isEnabled());
    }

    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstRemoteStationVax)
#include "tst_remote_station_vax.moc"
