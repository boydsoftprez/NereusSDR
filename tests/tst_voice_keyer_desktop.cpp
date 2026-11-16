// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original tests.
// =================================================================
// tests/tst_voice_keyer_desktop.cpp  (NereusSDR)
// =================================================================
//
// The desktop voice keyer (recording plan Task 12): the Voice keyer panel,
// VOICE in the TUNE / MOX row and the microphone clip recorder, against a
// fake recordings access and a fake microphone. Nothing keys a radio and no
// sound device opens.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
//   2026-11-16 - J.J. Boyd (KG4VCF), the older Core's VOICE dims whole and
//                 held elsewhere the box's messages dim. AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QAction>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

#include <cmath>
#include <memory>
#include <vector>

#include "core/recordings/AudioFileReader.h"
#include "fakes/FakeRecordingsAccess.h"
#include "gui/applets/TxKeyingSection.h"
#include "gui/applets/VoiceKeyerApplet.h"
#include "gui/recordings/LocalMicClipRecorder.h"
#include "gui/recordings/VoiceButton.h"

using namespace NereusSDR;

namespace {

const QString kHeld = QStringLiteral("iPhone has the transmitter. Take it to transmit from here.");

// The `voiceKeyer` document: `filled` messages (1-based) with names, and
// what goes out.
QJsonObject voiceDocument(const QList<int>& filled, const QJsonValue& sending = QJsonValue::Null,
                          bool repeat = false, int delaySec = 5)
{
    QJsonArray slotList;
    for (int n = 1; n <= 8; ++n) {
        const bool has = filled.contains(n);
        slotList.append(QJsonObject{{QStringLiteral("n"), n},
                                    {QStringLiteral("name"), has ? QStringLiteral("Msg %1").arg(n) : QString()},
                                    {QStringLiteral("lengthMs"), has ? 6000 : 0},
                                    {QStringLiteral("empty"), !has}});
    }
    QJsonObject doc = FakeRecordingsAccess::document({}, 1);
    doc.insert(QStringLiteral("voiceKeyer"), QJsonObject{{QStringLiteral("slots"), slotList},
                                                          {QStringLiteral("repeat"), repeat},
                                                          {QStringLiteral("repeatDelaySec"), delaySec},
                                                          {QStringLiteral("sending"), sending}});
    return doc;
}

QJsonObject sending(int n, qint64 positionMs, qint64 waitingMs = 0)
{
    return QJsonObject{{QStringLiteral("n"), n},
                       {QStringLiteral("positionMs"), positionMs},
                       {QStringLiteral("lengthMs"), 6000},
                       {QStringLiteral("waitingMs"), waitingMs},
                       {QStringLiteral("by"), QStringLiteral("Desktop")},
                       {QStringLiteral("deviceId"), QStringLiteral("station")}};
}

class FakeMic final : public MicClipSource {
public:
    struct Shared {
        std::vector<float> samples;
        size_t at = 0;
        int rate = 48000;
        bool open = false;
    };
    explicit FakeMic(std::shared_ptr<Shared> s) : m_s(std::move(s)) {}
    void open() override { m_s->open = true; }
    void close() override { m_s->open = false; }
    int read(float* dst, int n) override
    {
        const size_t take = std::min(static_cast<size_t>(n), m_s->samples.size() - m_s->at);
        std::copy_n(m_s->samples.begin() + static_cast<std::ptrdiff_t>(m_s->at), take, dst);
        m_s->at += take;
        return static_cast<int>(take);
    }
    int sampleRateHz() const override { return m_s->rate; }

private:
    std::shared_ptr<Shared> m_s;
};

std::vector<float> tone(int frames, float amplitude = 0.5f)
{
    std::vector<float> out(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        out[static_cast<size_t>(i)] = amplitude * std::sin(static_cast<float>(i) * 0.05f);
    }
    return out;
}

struct Row {
    QWidget host;
    TxKeyingSection* section = nullptr;
    FakeRecordingsAccess access;
    VoiceButton* voice = nullptr;
    qint64 clock = 0;
    Row()
    {
        access.version = 3;
        section = new TxKeyingSection(&host, 2, QMargins());
        auto* column = new QVBoxLayout(&host);
        column->addWidget(section);
        column->addStretch(1);
        host.resize(300, 120);
        host.move(50, 400);   // room above the row for the box
        access.setDocument(voiceDocument({1, 2, 3, 4}));
        voice = new VoiceButton(section, &access);
        voice->setClockForTest([this]() { return clock; });
        host.show();
    }
};

struct Panel {
    FakeRecordingsAccess access;
    VoiceKeyerApplet applet{nullptr, nullptr};
    std::shared_ptr<FakeMic::Shared> mic = std::make_shared<FakeMic::Shared>();
    Panel()
    {
        access.version = 3;
        access.setDocument(voiceDocument({1, 2, 3, 4}));
        applet.setAccess(&access);
        applet.setMicSourceFactory([this]() { return std::make_unique<FakeMic>(mic); });
        applet.resize(306, 260);
        applet.show();
    }
    QAction* menuAction(QMenu& menu, const QString& name)
    {
        for (QAction* a : menu.actions()) {
            if (a->objectName() == name) {
                return a;
            }
        }
        return nullptr;
    }
};

}  // namespace

class TstVoiceKeyerDesktop : public QObject {
    Q_OBJECT

private slots:
    // ---- The panel.

    void clickingAMessageSendsIt()
    {
        Panel p;
        p.applet.messageButtonForTest(2)->click();
        QCOMPARE(p.access.verbs(), QStringList{QStringLiteral("sendVoice")});
        QCOMPARE(p.access.calls.last().sliceId, 2);
        QCOMPARE(p.applet.messageButtonForTest(2)->text(), QStringLiteral("Msg 2"));
        QCOMPARE(p.applet.messageButtonForTest(6)->text(), QStringLiteral("M6"));
    }

    void aMessageGoingOutShowsProgressAndItsClickStopsIt()
    {
        Panel p;
        qint64 clock = 0;
        p.applet.setClockForTest([&clock]() { return clock; });
        p.access.setDocument(voiceDocument({1, 2, 3, 4}, sending(2, 3000)));
        QFrame* progress = p.applet.messageProgressForTest(2);
        QVERIFY(!progress->isHidden());
        const int half = p.applet.messageButtonForTest(2)->width() / 2;
        QVERIFY2(std::abs(progress->width() - half) <= 1, qPrintable(QString::number(progress->width())));
        QVERIFY(p.applet.statusText().startsWith(QStringLiteral("Sending Msg 2")));
        p.applet.messageButtonForTest(2)->click();
        QCOMPARE(p.access.verbs(), QStringList{QStringLiteral("stopVoice")});
        p.applet.stopButtonForTest()->click();
        QCOMPARE(p.access.verbs().last(), QStringLiteral("stopVoice"));
    }

    void importFillsTheFirstEmptyMessage()
    {
        Panel p;
        p.applet.setOpenChooserForTest([]() { return QStringLiteral("/tmp/Contest CQ.wav"); });
        p.applet.importButtonForTest()->click();
        QCOMPARE(p.access.verbs(), QStringList{QStringLiteral("importVoice")});
        QCOMPARE(p.access.calls.last().sliceId, 5);
        QCOMPARE(p.access.calls.last().id, QStringLiteral("/tmp/Contest CQ.wav=Contest CQ"));

        p.access.calls.clear();
        p.access.setDocument(voiceDocument({1, 2, 3, 4, 5, 6, 7, 8}));
        p.applet.importButtonForTest()->click();
        QVERIFY(p.access.calls.isEmpty());
        QCOMPARE(p.applet.statusForTest()->text(), QStringLiteral("All eight messages are full."));
    }

    void aMessageOnTheAirKeepsClearAndLoadDisabledWithTheReason()
    {
        Panel p;
        p.access.setDocument(voiceDocument({1, 2, 3, 4}, sending(2, 0, 4000)));   // waiting to repeat
        QMenu menu;
        p.applet.populateMessageMenu(&menu, 2);
        QAction* clear = p.menuAction(menu, QStringLiteral("VoiceClear"));
        QAction* load = p.menuAction(menu, QStringLiteral("VoiceLoad"));
        QAction* rename = p.menuAction(menu, QStringLiteral("VoiceRename"));
        QAction* listen = p.menuAction(menu, QStringLiteral("VoiceListen"));
        QAction* save = p.menuAction(menu, QStringLiteral("VoiceSave"));
        QVERIFY(clear && load && rename && listen && save);
        QVERIFY(!clear->isEnabled());
        QVERIFY(!load->isEnabled());
        QCOMPARE(clear->toolTip(), QStringLiteral("M2 is on the air."));
        QCOMPARE(load->toolTip(), QStringLiteral("M2 is on the air."));
        QVERIFY(rename->isEnabled());
        QVERIFY(listen->isEnabled());
        QVERIFY(save->isEnabled());

        p.applet.setNameAskerForTest([](int, const QString&) { return std::optional<QString>(QStringLiteral("CQ")); });
        rename->trigger();
        listen->trigger();
        QCOMPARE(p.access.verbs(), (QStringList{QStringLiteral("renameVoice"), QStringLiteral("listenVoice")}));
        QCOMPARE(p.access.calls.first().id, QStringLiteral("CQ"));

        QMenu other;
        p.applet.populateMessageMenu(&other, 3);
        QAction* clear3 = p.menuAction(other, QStringLiteral("VoiceClear"));
        QVERIFY(clear3->isEnabled());
        clear3->trigger();
        QCOMPARE(p.access.verbs().last(), QStringLiteral("clearVoice"));
        QCOMPARE(p.access.calls.last().sliceId, 3);
    }

    void recordingIntoAMessageSendsTheClipToTheCore()
    {
        Panel p;
        p.mic->samples = tone(48000);   // one second
        p.applet.recordButtonForTest()->click();
        QVERIFY(p.applet.armedForTest());
        p.applet.messageButtonForTest(6)->click();
        QCOMPARE(p.applet.recordingSlotForTest(), 6);
        QVERIFY(p.mic->open);
        QVERIFY(p.access.calls.isEmpty());   // recording keys nothing and sends nothing yet
        p.applet.recorderForTest()->pumpForTest();
        QVERIFY(p.applet.levelForTest()->value() > 0);
        p.applet.messageButtonForTest(6)->click();   // a second click ends it
        QCOMPARE(p.applet.recordingSlotForTest(), 0);
        QVERIFY(!p.mic->open);
        QCOMPARE(p.access.verbs(), QStringList{QStringLiteral("importVoice")});
        QCOMPARE(p.access.calls.last().sliceId, 6);
        const QString path = p.access.calls.last().id.section(QLatin1Char('='), 0, 0);
        QVERIFY(QFileInfo::exists(path));
        emit p.access.voiceImportFinished(6, true, QString());
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(p.applet.statusForTest()->text(), QStringLiteral("Saved M6."));
    }

    void aRefusedSendShowsTheReason()
    {
        Panel p;
        p.applet.messageButtonForTest(1)->click();
        emit p.access.refused(QStringLiteral("The radio is already transmitting."));
        QCOMPARE(p.applet.statusForTest()->text(), QStringLiteral("The radio is already transmitting."));
    }

    void heldElsewhereDimsTheSendsWithTakeTransmit()
    {
        Panel p;
        QSignalSpy take(&p.applet, &VoiceKeyerApplet::takeTransmitRequested);
        p.applet.setTransmitPermitted(false, kHeld);
        p.applet.setTakeTransmitOffered(true);
        QVERIFY(!p.applet.takeTransmitForTest()->isHidden());
        QCOMPARE(p.applet.messageButtonForTest(1)->toolTip(), kHeld);
        p.applet.messageButtonForTest(1)->click();
        QVERIFY(p.access.calls.isEmpty());
        QCOMPARE(p.applet.statusForTest()->text(), kHeld);
        p.applet.takeTransmitForTest()->click();
        QCOMPARE(take.count(), 1);
    }

    void anOlderCoreLeavesThePanelDisabledWithTheReason()
    {
        Panel p;
        p.access.version = 2;
        p.access.setDocument(voiceDocument({1}));
        QVERIFY(!p.applet.noteForTest()->isHidden());
        QCOMPARE(p.applet.noteForTest()->text(), QStringLiteral("This Core needs an update for recordings."));
        QVERIFY(!p.applet.messageButtonForTest(1)->isEnabled());
        QVERIFY(p.applet.messageButtonForTest(1)->isVisible());
        QVERIFY(p.applet.importButtonForTest()->isVisible());
    }

    // ---- VOICE.

    void voiceIsTheThirdButtonInTheRow()
    {
        Row r;
        QPushButton* voice = r.section->voiceButton();
        QVERIFY(voice);
        QCOMPARE(voice->height(), 22);
        QCOMPARE(voice->text(), QStringLiteral("VOICE"));
        QVERIFY(voice->x() > r.section->moxButton()->x());
        QCOMPARE(voice->y(), r.section->moxButton()->y());
        QVERIFY(std::abs(voice->width() - r.section->moxButton()->width()) <= 1);
    }

    void clickOpensTheBoxAndAMessageSendsAndCloses()
    {
        Row r;
        r.section->voiceButton()->click();
        QVERIFY(r.voice->boxOpen());
        QVERIFY(r.voice->boxForTest()->geometry().bottom()
                < r.section->voiceButton()->mapToGlobal(QPoint(0, 0)).y());
        r.voice->tileForTest(2)->click();
        QCOMPARE(r.access.verbs(), QStringList{QStringLiteral("sendVoice")});
        QCOMPARE(r.access.calls.last().sliceId, 2);
        QVERIFY(!r.voice->boxOpen());
    }

    void anEmptyMessageAsksForThePanel()
    {
        Row r;
        QSignalSpy panel(r.voice, &VoiceButton::panelRequested);
        r.voice->openBox();
        r.voice->tileForTest(5)->click();
        QCOMPARE(panel.count(), 1);
        QCOMPARE(panel.first().first().toInt(), 5);
        QVERIFY(r.access.calls.isEmpty());
        r.voice->openBox();
        r.voice->panelLinkForTest()->click();
        QCOMPARE(panel.count(), 2);
        QCOMPARE(panel.last().first().toInt(), 0);
    }

    void sendingTurnsVoiceRedWithTheMessageAndItsFill()
    {
        Row r;
        r.access.setDocument(voiceDocument({1, 2, 3, 4}, sending(2, 1000)));
        QCOMPARE(r.section->voiceButton()->text(), QStringLiteral("M2 0:01"));
        QCOMPARE(r.section->voiceButton()->property("look").toString(), QStringLiteral("sending"));
        QVERIFY(std::abs(r.voice->fillForTest() - 1.0 / 6.0) < 0.01);
        r.clock += 2000;   // between the Core's updates, it counts on
        r.voice->refreshForTest();
        QCOMPARE(r.section->voiceButton()->text(), QStringLiteral("M2 0:03"));
        QVERIFY(std::abs(r.voice->fillForTest() - 0.5) < 0.01);

        r.access.setDocument(voiceDocument({1, 2, 3, 4}, sending(2, 6000, 5000)));
        QCOMPARE(r.section->voiceButton()->text(), QStringLiteral("M2 again in 5 s"));
        QCOMPARE(r.section->voiceButton()->property("look").toString(), QStringLiteral("waiting"));
    }

    void voiceEscapeAndMoxStopIt()
    {
        Row r;
        r.access.setDocument(voiceDocument({1, 2, 3, 4}, sending(2, 1000)));
        r.section->voiceButton()->click();
        QCOMPARE(r.access.verbs(), QStringList{QStringLiteral("stopVoice")});
        QVERIFY(!r.voice->boxOpen());
        QTest::keyClick(r.section->voiceButton(), Qt::Key_Escape);
        QCOMPARE(r.access.verbs().size(), 2);
        QCOMPARE(r.access.verbs().last(), QStringLiteral("stopVoice"));
        r.section->moxButton()->click();
        QCOMPARE(r.access.verbs().size(), 3);
        QCOMPARE(r.access.verbs().last(), QStringLiteral("stopVoice"));

        // Nothing going out: MOX is MOX and Escape is Escape.
        r.access.calls.clear();
        r.access.setDocument(voiceDocument({1, 2, 3, 4}));
        r.section->moxButton()->click();
        QTest::keyClick(r.section->voiceButton(), Qt::Key_Escape);
        QVERIFY(r.access.calls.isEmpty());
    }

    void aRefusedStopShowsTheHoldersReason()
    {
        Row r;
        QSignalSpy shown(r.voice, &VoiceButton::refusalShown);
        r.access.setDocument(voiceDocument({1, 2, 3, 4}, sending(2, 1000)));
        r.section->voiceButton()->click();
        const QString reason = QStringLiteral("iPhone has the transmitter. Take it to stop the transmission.");
        emit r.access.refused(reason);
        QCOMPARE(shown.count(), 1);
        QCOMPARE(shown.first().first().toString(), reason);
        // A refusal this button did not ask for is not its to show.
        emit r.access.refused(QStringLiteral("Something else."));
        QCOMPARE(shown.count(), 1);
    }

    void heldElsewhereDimsVoiceAndOffersTakeTransmit()
    {
        Row r;
        QSignalSpy take(r.voice, &VoiceButton::takeTransmitRequested);
        r.voice->setTransmitPermitted(false, kHeld);
        r.voice->setTakeTransmitOffered(true);
        QPushButton* voice = r.section->voiceButton();
        QVERIFY(voice->isEnabled());   // it still opens the box that says why
        QVERIFY(voice->graphicsEffect() != nullptr);
        QCOMPARE(voice->toolTip(), kHeld);
        r.voice->openBox();
        QVERIFY(!r.voice->tileForTest(1)->isEnabled());
        QCOMPARE(r.voice->tileForTest(1)->toolTip(), kHeld);
        // The messages that would key dim, as the mockup's; Take transmit does not.
        for (int n = 1; n <= 4; ++n) {
            auto* effect = qobject_cast<QGraphicsOpacityEffect*>(r.voice->tileForTest(n)->graphicsEffect());
            QVERIFY2(effect != nullptr, qPrintable(QString::number(n)));
            QCOMPARE(effect->opacity(), 0.42);
        }
        QVERIFY(r.voice->takeTransmitForTest()->graphicsEffect() == nullptr);
        QVERIFY(r.voice->tileForTest(5)->isEnabled());   // an empty one still opens the panel
        QVERIFY(r.voice->tileForTest(5)->graphicsEffect() == nullptr);
        QCOMPARE(r.voice->holderNoteForTest()->text(), kHeld);
        QVERIFY(r.voice->takeTransmitForTest()->isVisible());
        r.voice->takeTransmitForTest()->click();
        QCOMPARE(take.count(), 1);
        QVERIFY(r.access.calls.isEmpty());

        r.voice->setTransmitPermitted(true, QString());
        QVERIFY(voice->graphicsEffect() == nullptr);
        QVERIFY(r.voice->tileForTest(1)->isEnabled());
        QVERIFY(r.voice->tileForTest(1)->graphicsEffect() == nullptr);
        r.voice->closeBox();
    }

    void anOlderCoreLeavesVoiceVisibleAndDisabled()
    {
        Row r;
        r.access.version = 2;
        r.access.setDocument(voiceDocument({1}));
        QPushButton* voice = r.section->voiceButton();
        QVERIFY(voice->isVisible());
        QVERIFY(!voice->isEnabled());
        QCOMPARE(voice->toolTip(), QStringLiteral("This Core needs an update for recordings."));
        QCOMPARE(voice->accessibleDescription(), QStringLiteral("This Core needs an update for recordings."));
        // All of it dims, label and box with the icon, as the mockup's #vBtn.off.
        auto* effect = qobject_cast<QGraphicsOpacityEffect*>(voice->graphicsEffect());
        QVERIFY(effect != nullptr);
        QCOMPARE(effect->opacity(), 0.45);
    }

    void repeatInTheBoxSetsIt()
    {
        Row r;
        r.access.setDocument(voiceDocument({1, 2}, QJsonValue::Null, false, 7));
        r.voice->openBox();
        r.voice->repeatForTest()->click();
        QCOMPARE(r.access.verbs(), QStringList{QStringLiteral("setVoiceRepeat")});
        QCOMPARE(r.access.calls.last().sliceId, 1);
        QCOMPARE(r.access.calls.last().id, QStringLiteral("7"));
        r.voice->closeBox();
    }

    // ---- The clip recorder.

    void theClipIs48kMono16Bit()
    {
        auto mic = std::make_shared<FakeMic::Shared>();
        mic->samples = tone(24000);
        LocalMicClipRecorder recorder(std::make_unique<FakeMic>(mic));
        QSignalSpy done(&recorder, &LocalMicClipRecorder::finished);
        recorder.start();
        recorder.pumpForTest();
        recorder.stop();
        QCOMPARE(done.count(), 1);
        const QString path = done.first().at(0).toString();
        QCOMPARE(done.first().at(1).toLongLong(), qint64(500));
        QCOMPARE(path, recorder.wavPath());
        QString error;
        std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        QCOMPARE(reader->sampleRateHz(), 48000);
        QCOMPARE(reader->channels(), 1);
        QCOMPARE(reader->lengthFrames(), qint64(24000));
        QFile wav(path);
        QVERIFY(wav.open(QIODevice::ReadOnly));
        const QByteArray header = wav.read(44);
        QCOMPARE(static_cast<int>(static_cast<quint8>(header[34])), 16);   // bits per sample
    }

    void anotherRateIsResampledTo48k()
    {
        auto mic = std::make_shared<FakeMic::Shared>();
        mic->rate = 44100;
        mic->samples = tone(44100);
        LocalMicClipRecorder recorder(std::make_unique<FakeMic>(mic));
        QSignalSpy done(&recorder, &LocalMicClipRecorder::finished);
        recorder.start();
        recorder.pumpForTest();
        recorder.stop();
        QCOMPARE(done.count(), 1);
        QString error;
        std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(recorder.wavPath(), &error);
        QVERIFY(reader);
        QCOMPARE(reader->sampleRateHz(), 48000);
        QCOMPARE(reader->channels(), 1);
        QVERIFY2(std::abs(reader->lengthFrames() - 48000) < 2000, qPrintable(QString::number(reader->lengthFrames())));
    }

    void theClipStopsItselfAtTwoMinutes()
    {
        auto mic = std::make_shared<FakeMic::Shared>();
        mic->samples = tone(48000 * 125, 0.1f);
        LocalMicClipRecorder recorder(std::make_unique<FakeMic>(mic));
        QSignalSpy done(&recorder, &LocalMicClipRecorder::finished);
        recorder.start();
        recorder.pumpForTest();
        QCOMPARE(done.count(), 1);
        QCOMPARE(done.first().at(1).toLongLong(), qint64(120000));
        QVERIFY(!recorder.isRecording());
        QVERIFY(!mic->open);
        QString error;
        std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(recorder.wavPath(), &error);
        QVERIFY(reader);
        QCOMPARE(reader->lengthFrames(), qint64(48000) * 120);
    }

    void nothingHeardIsSaid()
    {
        auto mic = std::make_shared<FakeMic::Shared>();
        LocalMicClipRecorder recorder(std::make_unique<FakeMic>(mic));
        QSignalSpy failed(&recorder, &LocalMicClipRecorder::failed);
        recorder.start();
        recorder.stop();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().toString(), LocalMicClipRecorder::nothingHeardReason());
    }
};

QTEST_MAIN(TstVoiceKeyerDesktop)
#include "tst_voice_keyer_desktop.moc"
