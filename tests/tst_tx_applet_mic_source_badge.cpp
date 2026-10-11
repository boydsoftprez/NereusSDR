// =================================================================
// tests/tst_tx_applet_mic_source_badge.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the TxApplet mic-source badge text for all three MicSource
// values: Pc -> "PC mic", Radio -> "Radio mic", Vax -> "VAX". Covers
// both the live micSourceChanged path and the syncFromModel path.
//
// Native audio plan Task 20 (R-AUD-24, R-AUD-09, R-AUD-13, settled call
// 35, V-SW-8, V-UI-3): the PC mic's role status turns the badge amber
// with "PC mic not connected" or "PC mic in use by another program" and
// its tooltip, a playing mic's tooltip names the device, a returning mic
// clears it with no click, other failures and the radio and VAX sources
// keep today's badge, and the source never moves. With
// NEREUS_AUDIO_SETUP_CAPTURE_DIR set it saves the normal, not connected
// and in use captures.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-10-09 - PC mic states (native audio plan Task 20) by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QLabel>
#include <QScopeGuard>
#include <QDir>
#include <QPixmap>
#include <QSignalSpy>

#include "gui/applets/TxApplet.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/styles/AppTheme.h"
#include "gui/StyleConstants.h"
#include "core/AudioEngine.h"
#include "core/audio/IAudioStreamHost.h"
#include "OperatorWording.h"
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

// The engine's TxInput role status, as the stream supervisor reports it.
AudioRoleStatus micStatus(AudioRoleState state, AudioRoleReason reason,
                          const QString& chosen, const QString& playing)
{
    AudioRoleStatus status;
    status.state = state;
    status.reason = reason;
    status.chosenName = chosen;
    status.playingName = playing;
    return status;
}

void reportMic(RadioModel& model, const AudioRoleStatus& status)
{
    emit model.localAudioDevices()->roleStatusChanged(AudioRole::TxInput, status);
}

bool isAmber(const QLabel* badge)
{
    return badge->styleSheet().contains(QLatin1String(Style::kAmberBg))
        && badge->styleSheet().contains(QLatin1String(Style::kAmberText));
}

const QString kNotConnectedTip = QStringLiteral(
    "USB Mic is not connected. Transmit audio is silent until it comes back; "
    "NereusSDR never switches to another mic by itself. Change the source in "
    "Settings > Audio > Microphone.");
const QString kInUseTip = QStringLiteral(
    "USB Mic is in use by another program. Transmit audio is silent until it "
    "comes back; NereusSDR never switches to another mic by itself. Change the "
    "source in Settings > Audio > Microphone.");
const QString kTodayTip = QStringLiteral(
    "Change microphone source via Settings > Audio > Microphone.");

void saveCapture(TxApplet& applet, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR");
    if (dir.isEmpty()) {
        return;
    }
    QDir().mkpath(dir);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    const QPixmap shot = applet.grab();
    const int scale = qRound(shot.devicePixelRatio());
    const QString path = QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(scale);
    QVERIFY2(shot.save(path), qPrintable(path));
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
        // Tests the VAX path itself, which Windows does not offer.
        model.transmitModel().setVaxSourceAvailable(true);
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        model.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
    }

    void syncFromModel_Vax()
    {
        RadioModel model;
        // Tests the VAX path itself, which Windows does not offer.
        model.transmitModel().setVaxSourceAvailable(true);
        model.transmitModel().setMicSource(MicSource::Vax);

        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);
        applet.syncFromModel();
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
    }

    // R-AUD-24: a PC mic that is not connected turns the badge amber with
    // its tooltip; the model reports the status and the source stays.
    void pcMicNotConnected_amberWithTooltip()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Pc);
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);
        QVERIFY(!isAmber(badge));
        QSignalSpy changed(&model, &RadioModel::pcMicStatusChanged);

        const AudioRoleStatus gone = micStatus(AudioRoleState::Silent,
            AudioRoleReason::NotConnected, QStringLiteral("USB Mic"), QString());
        reportMic(model, gone);
        QCOMPARE(changed.count(), 1);
        QVERIFY(model.pcMicStatus() == gone);
        QCOMPARE(badge->text(), QStringLiteral("PC mic not connected"));
        QCOMPARE(badge->toolTip(), kNotConnectedTip);
        QVERIFY(isAmber(badge));
        // R-AUD-09: never replaced; the source does not move.
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);

        // The same status again is no change.
        reportMic(model, gone);
        QCOMPARE(changed.count(), 1);
        // Another role's status is not the mic's.
        emit model.localAudioDevices()->roleStatusChanged(AudioRole::Speakers,
            micStatus(AudioRoleState::Playing, AudioRoleReason::None, QString(),
                      QStringLiteral("Desk speakers")));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(badge->text(), QStringLiteral("PC mic not connected"));
    }

    // Settled call 35: a busy mic says "in use by another program".
    void pcMicInUse_amberWithTooltip()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Pc);
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::InUse,
                                   QStringLiteral("USB Mic"), QString()));
        QCOMPARE(badge->text(), QStringLiteral("PC mic in use by another program"));
        QCOMPARE(badge->toolTip(), kInUseTip);
        QVERIFY(isAmber(badge));
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
        QVERIFY(OperatorWording::isPlain(badge->text()));
        QVERIFY(OperatorWording::isPlain(badge->toolTip()));
        QVERIFY(OperatorWording::isPlain(kNotConnectedTip));
    }

    // R-AUD-13: a playing mic's tooltip names the device; R-AUD-24: a
    // returning mic clears the amber state with no click.
    void pcMicReturns_clearsAmberAndNamesDevice()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Pc);
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::NotConnected,
                                   QStringLiteral("USB Mic"), QString()));
        QVERIFY(isAmber(badge));
        reportMic(model, micStatus(AudioRoleState::Playing, AudioRoleReason::None,
                                   QStringLiteral("USB Mic"), QStringLiteral("USB Mic")));
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));
        QCOMPARE(badge->toolTip(), QStringLiteral("PC mic: USB Mic"));
        QVERIFY(!isAmber(badge));

        // "(platform default)": no chosen name, the device in use is named.
        reportMic(model, micStatus(AudioRoleState::Playing, AudioRoleReason::None,
                                   QString(), QStringLiteral("MacBook Pro Microphone")));
        QCOMPARE(badge->toolTip(), QStringLiteral("PC mic: MacBook Pro Microphone"));
    }

    // Other mic failures keep today's badge (R-AUD-24: Retry stays for
    // them on the Microphone page).
    void otherFailure_keepsTodaysBadge()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Pc);
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::NoDevice,
                                   QString(), QString()));
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));
        QCOMPARE(badge->toolTip(), kTodayTip);
        QVERIFY(!isAmber(badge));

        reportMic(model, micStatus(AudioRoleState::Off, AudioRoleReason::None,
                                   QString(), QString()));
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));
        QCOMPARE(badge->toolTip(), kTodayTip);
        QVERIFY(!isAmber(badge));
    }

    // Mic source radio (or VAX): the badge never shows the PC mic states,
    // and switching back to the PC mic shows them at once.
    void radioAndVaxSources_neverShowPcMicStates()
    {
        RadioModel model;
        // Tests the VAX path itself, which Windows does not offer.
        model.transmitModel().setVaxSourceAvailable(true);
        model.transmitModel().setMicSource(MicSource::Radio);
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::NotConnected,
                                   QStringLiteral("USB Mic"), QString()));
        QCOMPARE(badge->text(), QStringLiteral("Radio mic"));
        QVERIFY(!isAmber(badge));
        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);

        model.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
        QVERIFY(!isAmber(badge));
        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::InUse,
                                   QStringLiteral("USB Mic"), QString()));
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
        QVERIFY(!isAmber(badge));

        model.transmitModel().setMicSource(MicSource::Pc);
        QCOMPARE(badge->text(), QStringLiteral("PC mic in use by another program"));
        QVERIFY(isAmber(badge));
        model.transmitModel().setMicSource(MicSource::Radio);
        QCOMPARE(badge->text(), QStringLiteral("Radio mic"));
        QVERIFY(!isAmber(badge));
    }

    // V-UI-3: normal, not connected and in use, against tx-mic-mockup.html.
    void captures()
    {
        if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
            QSKIP("NEREUS_AUDIO_SETUP_CAPTURE_DIR is not set");
        }
        const QPalette previousPalette = qApp->palette();
        const QString previousQss = qApp->styleSheet();
        const auto restoreTheme = qScopeGuard([previousPalette, previousQss]() {
            qApp->setPalette(previousPalette);
            qApp->setStyleSheet(previousQss);
        });
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Pc);
        TxApplet applet(&model);
        applet.setAttribute(Qt::WA_DontShowOnScreen);
        applet.resize(260, applet.sizeHint().height());
        applet.show();
        reportMic(model, micStatus(AudioRoleState::Playing, AudioRoleReason::None,
                                   QStringLiteral("USB Mic"), QStringLiteral("USB Mic")));
        saveCapture(applet, QStringLiteral("tx-mic-normal"));
        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::NotConnected,
                                   QStringLiteral("USB Mic"), QString()));
        saveCapture(applet, QStringLiteral("tx-mic-not-connected"));
        reportMic(model, micStatus(AudioRoleState::Silent, AudioRoleReason::InUse,
                                   QStringLiteral("USB Mic"), QString()));
        saveCapture(applet, QStringLiteral("tx-mic-in-use"));
    }
};

QTEST_MAIN(TstTxAppletMicSourceBadge)
#include "tst_tx_applet_mic_source_badge.moc"
