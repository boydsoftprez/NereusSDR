// no-port-check: NereusSDR-original unit-test file. No Thetis logic is
// ported here.
// =================================================================
// tests/tst_audio_advanced_page.cpp  (NereusSDR)
// =================================================================
//
// Setup > Audio > Advanced, "Reset all audio to defaults" (R-R3-10,
// R-R3-23).
//
// Locally the button calls AudioEngine::resetAudioSettings(): every
// audio/* key AppSettings lists is removed and the VAX outputs are made
// again. In a remote window both halves are wrong. AppSettings there also
// lists the keys the Core holds (audio/DspRate and audio/DspBlockSize
// classify Station), so removing "every audio/* key" would send removes to
// the Core. A remote Reset removes only this computer's audio/* keys and
// rebuilds its local VAX outputs, which carry remote receiver audio.
//
// The confirm box is modal (QMessageBox::exec), so each case answers it
// from a timer the way an operator would, by clicking its button.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23 -- New test file for the R3 remote window Setup plan,
//                 Task 1. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>

#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/settings/SettingsProxy.h"
#include "gui/setup/AudioAdvancedPage.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

using namespace NereusSDR;

namespace {

// A VAX slot occupant that says when AudioEngine throws it away, so a
// reset's output rebuild is observed on the slot as well as its signal.
class WatchedVaxBus final : public FakeAudioBus {
public:
    explicit WatchedVaxBus(bool* destroyed) : m_destroyed(destroyed) {}
    ~WatchedVaxBus() override { *m_destroyed = true; }

private:
    bool* m_destroyed;
};

// Answers the Reset confirm box with its "Reset all audio" button as soon
// as the box is up. Returns a flag that says whether it was answered.
std::shared_ptr<bool> answerResetConfirm(QObject* context)
{
    auto answered = std::make_shared<bool>(false);
    auto* timer = new QTimer(context);
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, context, [timer, answered] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr) { return; }
        for (QAbstractButton* button : box->buttons()) {
            if (button->text() == QStringLiteral("Reset all audio")) {
                *answered = true;
                timer->stop();
                timer->deleteLater();
                button->click();
                return;
            }
        }
    });
    timer->start();
    return answered;
}

QPushButton* resetButtonOf(QWidget* page)
{
    for (QPushButton* button : page->findChildren<QPushButton*>()) {
        if (button->text().startsWith(QStringLiteral("Reset all audio to defaults"))) {
            return button;
        }
    }
    return nullptr;
}

// This computer's audio keys, plus one key outside audio/ that Reset has
// always kept.
void seedThisComputersKeys()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("audio/Speakers/DeviceName"), QStringLiteral("Desk speakers"));
    s.setValue(QStringLiteral("audio/TxInput/DeviceName"), QStringLiteral("Desk mic"));
    s.setValue(QStringLiteral("audio/Vax1/DeviceName"), QStringLiteral("CABLE-A"));
    s.setValue(QStringLiteral("audio/VacFeedback/1/Gain"), QStringLiteral("1.5000"));
    s.setValue(QStringLiteral("audio/SendIqToVax"), QStringLiteral("True"));
    s.setValue(QStringLiteral("audio/FirstRunComplete"), QStringLiteral("True"));
    s.setValue(QStringLiteral("tx/OwnerSlot"), QStringLiteral("0"));
}

} // namespace

class TstAudioAdvancedPage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("audio-advanced-page-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // A remote window: this computer's audio/* keys go, the Core's stay
    // and nothing is sent to it. Its local VAX output is rebuilt.
    void remoteResetTouchesOnlyThisComputersKeys()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("audio/DspRate"), QStringLiteral("96000")},
            {QStringLiteral("audio/DspBlockSize"), QStringLiteral("512")},
        });
        AppSettings::instance().setRemoteBackend(&proxy);
        seedThisComputersKeys();

        RadioModel remote(RadioModel::Role::Remote);
        AudioEngine* const engine = remote.audioEngine();
        bool vaxBusDestroyed = false;
        engine->setVaxBusForTest(1, std::make_unique<WatchedVaxBus>(&vaxBusDestroyed));

        AudioAdvancedPage page(&remote);
        QPushButton* const reset = resetButtonOf(&page);
        QVERIFY(reset != nullptr);

        QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&proxy, &SettingsProxy::outboundRemoveRequested);
        QSignalSpy vaxChanged(engine, &AudioEngine::vaxConfigChanged);

        const std::shared_ptr<bool> answered = answerResetConfirm(this);
        reset->click();
        QVERIFY(*answered);

        auto& s = AppSettings::instance();
        for (const char* key : {"audio/Speakers/DeviceName", "audio/TxInput/DeviceName",
                                "audio/Vax1/DeviceName", "audio/VacFeedback/1/Gain",
                                "audio/SendIqToVax", "audio/FirstRunComplete"}) {
            QVERIFY2(!s.contains(QString::fromLatin1(key)), key);
        }
        QCOMPARE(s.value(QStringLiteral("tx/OwnerSlot")).toString(), QStringLiteral("0"));

        // The Core's keys: still there, and nothing crossed the wire.
        QCOMPARE(s.value(QStringLiteral("audio/DspRate")).toString(), QStringLiteral("96000"));
        QCOMPARE(s.value(QStringLiteral("audio/DspBlockSize")).toString(), QStringLiteral("512"));
        QCOMPARE(writes.count(), 0);
        QCOMPARE(removes.count(), 0);

        // The old local output is retired and all four slots announce their
        // rebuilt defaults, just as they do in direct mode.
        QCOMPARE(vaxChanged.count(), 4);
        QVERIFY(vaxBusDestroyed);
    }

    // Local direct mode: the Reset it has always done. Every audio/* key,
    // and the VAX slots are made again.
    void localResetIsUnchanged()
    {
        seedThisComputersKeys();
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/DspRate"), QStringLiteral("96000"));
        s.setValue(QStringLiteral("audio/DspBlockSize"), QStringLiteral("512"));

        RadioModel local;
        AudioEngine* const engine = local.audioEngine();
        bool vaxBusDestroyed = false;
        engine->setVaxBusForTest(1, std::make_unique<WatchedVaxBus>(&vaxBusDestroyed));

        AudioAdvancedPage page(&local);
        QPushButton* const reset = resetButtonOf(&page);
        QVERIFY(reset != nullptr);
        QSignalSpy vaxChanged(engine, &AudioEngine::vaxConfigChanged);
        QSignalSpy resetDone(engine, &AudioEngine::audioSettingsReset);

        const std::shared_ptr<bool> answered = answerResetConfirm(this);
        reset->click();
        QVERIFY(*answered);

        for (const QString& key : s.allKeys()) {
            QVERIFY2(!key.startsWith(QStringLiteral("audio/")), qPrintable(key));
        }
        QCOMPARE(s.value(QStringLiteral("tx/OwnerSlot")).toString(), QStringLiteral("0"));
        QCOMPARE(vaxChanged.count(), 4);
        QVERIFY(vaxBusDestroyed);
        QCOMPARE(resetDone.count(), 1);
    }
};

QTEST_MAIN(TstAudioAdvancedPage)
#include "tst_audio_advanced_page.moc"
