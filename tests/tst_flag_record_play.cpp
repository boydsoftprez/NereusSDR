// =================================================================
// tests/tst_flag_record_play.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test.
//
// Recording plan Task 6 (design sections 3, 6.2, 6.3, V1, V9): Record and
// Play on the slice flag, driven against a fake RecordingsAccess.
//   - The side buttons run x, lock, Record, Play, then the more button, 20 by
//     20 on a 22 step; Slice A has no x slot.
//   - Not connected and an older Core: both buttons shown, disabled, with
//     the reason as tooltip and accessible description.
//   - Record asks the Core to start and stop; its look and label follow the
//     document (recording, "+ your voice", armed).
//   - Play plays the slice's latest contact recording and stops it; with
//     nothing recorded it is disabled with the reason; the playing label.
//   - The right-click list: the slice's last eight, All recordings..., Show
//     in Finder (disabled with the reason when the Core is elsewhere),
//     Recording settings....
//   - The Kept notice: name, length, the reason in words, Play, for 6 s.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 6.
// =================================================================

#include <QAction>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QTimer>

#include <memory>

#include "fakes/FakeRecordingsAccess.h"
#include "gui/recordings/FlagRecordPlay.h"
#include "gui/recordings/RevealFile.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

RecordingEntry contact(const QString& id, int slice, const QDateTime& start, qint64 lengthMs,
                       qint64 hz = 7236400, const QString& call = QStringLiteral("K4ABC"))
{
    RecordingEntry e;
    e.id = id;
    e.kind = RecordingKind::Contact;
    e.sliceId = slice;
    e.startUtc = start;
    e.lengthMs = lengthMs;
    e.frequencyHz = hz;
    e.mode = QStringLiteral("LSB");
    e.call = call;
    e.name = recordingBaseName(start, hz, e.mode, call);
    return e;
}

const QDateTime kNow(QDate(2026, 11, 15), QTime(14, 40), QTimeZone::utc());

struct Flag {
    QWidget pan;
    VfoWidget* flag{nullptr};
    FakeRecordingsAccess access;
    std::unique_ptr<FlagRecordPlay> rp;

    explicit Flag(int slice, bool withAccess = true)
    {
        pan.resize(900, 500);
        flag = new VfoWidget(&pan);
        flag->setSliceIndex(slice);
        rp = std::make_unique<FlagRecordPlay>(flag, withAccess ? &access : nullptr, slice);
        rp->setClockForTest([] { return kNow; });
        pan.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pan));
        flag->updatePosition(450, 20, VfoWidget::FlagDir::ForceRight);
    }
    QPushButton* rec() const { return flag->recordButton(); }
    QPushButton* play() const { return flag->playButton(); }
    void ready(const QJsonArray& slices, qint64 revision = 1, const QString& folder = {})
    {
        access.setDocument(FakeRecordingsAccess::document(slices, revision, folder));
    }
};

QStringList actionTexts(QMenu& menu)
{
    QStringList out;
    for (QAction* a : menu.actions()) {
        out.append(a->isSeparator() ? QStringLiteral("-") : a->text());
    }
    return out;
}

QAction* actionStarting(QMenu& menu, const QString& start)
{
    for (QAction* a : menu.actions()) {
        if (a->text().startsWith(start)) {
            return a;
        }
    }
    return nullptr;
}

}  // namespace

class TestFlagRecordPlay : public QObject {
    Q_OBJECT

private slots:
    void the_side_buttons_run_close_lock_record_play_more()
    {
        for (int slice : {1, 0}) {
            Flag f(slice);
            QList<QPushButton*> order;
            if (slice != 0) {
                order.append(f.flag->closeButtonForTest());
            } else {
                QVERIFY(!f.flag->closeButtonForTest()->isVisible());
            }
            order += {f.flag->lockButtonForTest(), f.rec(), f.play(), f.flag->moreButtonForTest()};
            for (int i = 0; i < order.size(); ++i) {
                QPushButton* b = order.at(i);
                QVERIFY(b != nullptr);
                QVERIFY2(b->isVisible(), qPrintable(QStringLiteral("slice %1 button %2").arg(slice).arg(i)));
                QCOMPARE(b->size(), QSize(20, 20));
                QCOMPARE(b->x(), order.first()->x());
                QCOMPARE(b->y(), f.flag->y() + 22 * i);
            }
            QVERIFY(f.flag->occupiedRegion().contains(f.rec()->geometry()));
            QVERIFY(f.flag->occupiedRegion().contains(f.play()->geometry()));
        }
    }

    void not_connected_and_an_older_core_disable_both_with_the_reason()
    {
        {
            Flag f(0, false);
            for (QPushButton* b : {f.rec(), f.play()}) {
                QVERIFY(b->isVisible());
                QVERIFY(!b->isEnabled());
                QCOMPARE(b->toolTip(), QStringLiteral("Connect to a radio to use recordings."));
                QCOMPARE(b->accessibleDescription(), QStringLiteral("Connect to a radio to use recordings."));
            }
        }
        Flag f(1);
        f.access.setState(RecordingsAccess::State::NeedsNewerCore);
        for (QPushButton* b : {f.rec(), f.play()}) {
            QVERIFY(b->isVisible());
            QVERIFY(!b->isEnabled());
            QCOMPARE(b->toolTip(), QStringLiteral("This Core needs an update for recordings."));
            QCOMPARE(b->accessibleDescription(), QStringLiteral("This Core needs an update for recordings."));
        }
        f.access.setState(RecordingsAccess::State::NotConnected);
        QCOMPARE(f.rec()->toolTip(), QStringLiteral("Connect to a radio to use recordings."));
        // A click on a disabled button asks nothing of the Core.
        f.rec()->click();
        QVERIFY(f.access.calls.isEmpty());
    }

    void record_starts_and_stops_the_slice_on_the_core()
    {
        Flag f(0);
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("idle"))});
        QVERIFY(f.rec()->isEnabled());
        QCOMPARE(f.rp->recordLook(), FlagRecordPlay::RecordLook::Idle);
        QCOMPARE(f.rec()->toolTip(),
                 QStringLiteral("Record Slice A: what you hear, and your own voice when you transmit"));
        QVERIFY(!f.rp->recordLabelForTest()->isVisible());

        f.rec()->click();
        QCOMPARE(f.access.verbs().filter(QStringLiteral("start")).size(), 1);
        QCOMPARE(f.access.calls.last().sliceId, 0);
        // The button shows the Core's state, not the click.
        QVERIFY(!f.rec()->isChecked());

        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("recording"), 83000)});
        QCOMPARE(f.rp->recordLook(), FlagRecordPlay::RecordLook::Recording);
        QVERIFY(f.rec()->isChecked());
        QCOMPARE(f.rec()->toolTip(), QStringLiteral("Stop recording Slice A"));
        QLabel* label = f.rp->recordLabelForTest();
        QVERIFY(label->isVisible());
        QCOMPARE(label->text(), QStringLiteral("\u25CF 1:23"));
        // Beside the button, on the side away from the flag.
        QCOMPARE(label->geometry().right() + 1 + 3, f.rec()->x());
        QCOMPARE(label->y(), f.rec()->y() + 1);

        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("recording"), 84000, true)});
        QCOMPARE(label->text(), QStringLiteral("\u25CF 1:24  + your voice"));

        f.access.calls.clear();
        f.rec()->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("stop")});
        QCOMPARE(f.access.calls.last().sliceId, 0);
    }

    void armed_shows_the_ring_and_a_click_records_now()
    {
        Flag f(2);
        f.ready({FakeRecordingsAccess::slice(2, QStringLiteral("armed"))});
        QCOMPARE(f.rp->recordLook(), FlagRecordPlay::RecordLook::Armed);
        QCOMPARE(f.rp->recordLabelForTest()->text(), QStringLiteral("Waits for you to transmit"));
        QVERIFY(f.rp->recordLabelForTest()->isVisible());
        QVERIFY(!f.rec()->isChecked());
        f.rec()->click();
        QCOMPARE(f.access.verbs().filter(QStringLiteral("start")).size(), 1);
        QCOMPARE(f.access.calls.last().sliceId, 2);
    }

    void play_plays_the_latest_contact_recording_and_stops()
    {
        Flag f(0);
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("idle"))}, 1);
        QVERIFY(!f.play()->isEnabled());
        QCOMPARE(f.play()->toolTip(), QStringLiteral("Nothing recorded on Slice A yet."));
        QCOMPARE(f.play()->accessibleDescription(), QStringLiteral("Nothing recorded on Slice A yet."));

        RecordingEntry pan;
        pan.id = QStringLiteral("whole");
        pan.kind = RecordingKind::WholePan;
        pan.startUtc = kNow;
        f.access.entries = {pan, contact(QStringLiteral("b1"), 1, kNow.addSecs(-60), 5000),
                            contact(QStringLiteral("a2"), 0, kNow.addSecs(-1800), 205000),
                            contact(QStringLiteral("a1"), 0, kNow.addSecs(-5400), 64000)};
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("idle"))}, 2);
        QCOMPARE(f.access.calls.last().verb, QStringLiteral("requestList"));
        QCOMPARE(f.access.calls.last().count, FlagRecordPlay::kListRequestCount);
        QVERIFY(f.play()->isEnabled());
        QCOMPARE(f.rp->latestRecordingId(), QStringLiteral("a2"));

        f.access.calls.clear();
        f.play()->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("play")});
        QCOMPARE(f.access.calls.last().id, QStringLiteral("a2"));
        QVERIFY(!f.play()->isChecked());

        f.access.setPlayback(PlaybackState{QStringLiteral("local"), QStringLiteral("a2"), 12000, 205000, true, false});
        QVERIFY(f.rp->isPlaying());
        QVERIFY(f.play()->isChecked());
        QCOMPARE(f.play()->toolTip(), QStringLiteral("Stop playing"));
        QCOMPARE(f.rp->playLabelText(), QStringLiteral("7.236.400 LSB  K4ABC  0:12 / 3:25"));
        QVERIFY(f.rp->playLabelForTest()->isVisible());

        f.access.calls.clear();
        f.play()->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("stopPlay")});
        f.access.setPlayback(std::nullopt);
        QVERIFY(!f.play()->isChecked());
        QVERIFY(!f.rp->playLabelForTest()->isVisible());
        // The same revision asks for no new list.
        f.access.calls.clear();
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("idle"))}, 2);
        QVERIFY(f.access.calls.isEmpty());
    }

    void the_right_click_list_holds_the_slices_last_eight_and_the_actions()
    {
        Flag f(0);
        for (int i = 0; i < 10; ++i) {
            f.access.entries.append(contact(QStringLiteral("a%1").arg(i), 0, kNow.addSecs(-3600 * 12 * i),
                                            60000 + i * 1000, 7185000 + i * 1000,
                                            i % 2 == 0 ? QStringLiteral("K4ABC") : QString()));
        }
        f.access.entries.insert(3, contact(QStringLiteral("other"), 1, kNow, 1000));
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("idle"))}, 1, QStringLiteral("/tmp/rec"));
        QCOMPARE(f.rp->recentRecordings().size(), FlagRecordPlay::kMenuRecordings);

        QMenu menu;
        f.rp->populatePlayMenu(&menu);
        const QStringList texts = actionTexts(menu);
        QCOMPARE(texts.first(), QStringLiteral("Play on this computer's speaker"));
        QVERIFY(!menu.actions().first()->isEnabled());
        QCOMPARE(texts.at(1), QStringLiteral("7.185.000 LSB  K4ABC\ttoday 1440Z \u00B7 1:00"));
        QCOMPARE(texts.at(2), QStringLiteral("7.186.000 LSB\ttoday 0240Z \u00B7 1:01"));
        QCOMPARE(texts.at(3), QStringLiteral("7.187.000 LSB  K4ABC\tyesterday 1440Z \u00B7 1:02"));
        QCOMPARE(texts.at(8), QStringLiteral("7.192.000 LSB\t3 days ago 0240Z \u00B7 1:07"));
        QCOMPARE(texts.at(9), QStringLiteral("-"));
        QCOMPARE(texts.mid(10), (QStringList{QStringLiteral("All recordings\u2026"), RevealFile::actionText(),
                                             QStringLiteral("Recording settings\u2026")}));
        // Nothing opens the window or the settings yet: they wait, disabled.
        QVERIFY(!actionStarting(menu, QStringLiteral("All recordings"))->isEnabled());
        QVERIFY(!actionStarting(menu, QStringLiteral("Recording settings"))->isEnabled());

        f.access.calls.clear();
        menu.actions().at(2)->trigger();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("play")});
        QCOMPARE(f.access.calls.last().id, QStringLiteral("a1"));

        QString revealed;
        f.rp->setRevealForTest([&revealed](const QString& path) {
            revealed = path;
            return true;
        });
        QAction* reveal = actionStarting(menu, RevealFile::actionText());
        QVERIFY(reveal->isEnabled());
        reveal->trigger();
        QCOMPARE(revealed, QStringLiteral("/tmp/rec"));

        // Listened to, All recordings... and Recording settings... go out.
        QSignalSpy all(f.rp.get(), &FlagRecordPlay::allRecordingsRequested);
        QSignalSpy settings(f.rp.get(), &FlagRecordPlay::recordingSettingsRequested);
        QMenu second;
        f.rp->populatePlayMenu(&second);
        actionStarting(second, QStringLiteral("All recordings"))->trigger();
        actionStarting(second, QStringLiteral("Recording settings"))->trigger();
        QCOMPARE(all.size(), 1);
        QCOMPARE(settings.size(), 1);

        // The Core on another computer: Show in Finder stays, dimmed, with why.
        f.access.coreHere = false;
        QMenu third;
        f.rp->populatePlayMenu(&third);
        QAction* away = actionStarting(third, RevealFile::actionText());
        QVERIFY(!away->isEnabled());
        QCOMPARE(away->text(), RevealFile::actionText() + QStringLiteral("\tThe Core is on another computer."));
        QCOMPARE(away->toolTip(), QStringLiteral("The Core is on another computer."));

        // The recording playing now is marked.
        f.access.setPlayback(PlaybackState{QStringLiteral("local"), QStringLiteral("a0"), 0, 60000, true, false});
        QMenu fourth;
        f.rp->populatePlayMenu(&fourth);
        QVERIFY(fourth.actions().at(1)->text().startsWith(QStringLiteral("\u25B6 7.185.000")));
    }

    void the_kept_notice_says_why_and_plays()
    {
        Flag f(0);
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("idle"))});
        const QJsonObject kept{{QStringLiteral("seq"), 4},
                               {QStringLiteral("slice"), 0},
                               {QStringLiteral("id"), QStringLiteral("k1")},
                               {QStringLiteral("name"), QStringLiteral("2026-11-15 1432Z 7.236.400 LSB K4ABC")},
                               {QStringLiteral("lengthMs"), 83000},
                               {QStringLiteral("reason"), QStringLiteral("device")},
                               {QStringLiteral("device"), QStringLiteral("iPhone")}};
        // Another slice's recording leaves this flag alone.
        QJsonObject otherSlice = kept;
        otherSlice.insert(QStringLiteral("slice"), 1);
        emit f.access.kept(otherSlice);
        QVERIFY(!f.rp->keptNoticeForTest()->isVisible());

        emit f.access.kept(kept);
        QFrame* notice = f.rp->keptNoticeForTest();
        QVERIFY(notice->isVisible());
        const QString text = f.rp->keptTextForTest()->text();
        QVERIFY2(text.contains(QStringLiteral("Kept: 2026-11-15 1432Z 7.236.400 LSB K4ABC, 1:23")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("Stopped on the iPhone")), qPrintable(text));
        QCOMPARE(notice->y(), f.flag->y() + f.flag->height() + 10);
        QCOMPARE(f.rp->keptTimerForTest()->interval(), 6000);
        QVERIFY(f.rp->keptTimerForTest()->isActive());
        QVERIFY(f.rp->keptPlayButtonForTest()->isEnabled());

        f.access.calls.clear();
        f.rp->keptPlayButtonForTest()->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("play")});
        QCOMPARE(f.access.calls.last().id, QStringLiteral("k1"));
        QVERIFY(!notice->isVisible());

        // A new recording on the slice takes the notice away.
        emit f.access.kept(kept);
        QVERIFY(notice->isVisible());
        f.ready({FakeRecordingsAccess::slice(0, QStringLiteral("recording"), 1000)}, 2);
        QVERIFY(!notice->isVisible());
    }

    void the_kept_reasons_read_in_words()
    {
        const auto why = [](const QString& reason, int quiet = 2, bool here = true, const QString& device = {}) {
            return FlagRecordPlay::keptReasonText(QJsonObject{{QStringLiteral("reason"), reason},
                                                              {QStringLiteral("device"), device}},
                                                  quiet, here);
        };
        QCOMPARE(why(QStringLiteral("device"), 2, true, QStringLiteral("iPhone")),
                 QStringLiteral("Stopped on the iPhone"));
        QCOMPARE(why(QStringLiteral("device"), 2, true, QStringLiteral("JJ's iPad")),
                 QStringLiteral("Stopped on JJ's iPad"));
        QCOMPARE(why(QStringLiteral("device"), 2, true, QStringLiteral("Another device")),
                 QStringLiteral("Stopped on another device"));
        QCOMPARE(why(QStringLiteral("quiet")), QStringLiteral("Stopped by itself after two quiet minutes"));
        QCOMPARE(why(QStringLiteral("quiet"), 1), QStringLiteral("Stopped by itself after one quiet minute"));
        QCOMPARE(why(QStringLiteral("quiet"), 15), QStringLiteral("Stopped by itself after 15 quiet minutes"));
        QCOMPARE(why(QStringLiteral("diskFull")), QStringLiteral("Stopped: the disk is full"));
        QCOMPARE(why(QStringLiteral("wav")), QStringLiteral("Kept as WAV: the MP3 encoder failed"));
        QCOMPARE(why(QStringLiteral("here"), 2, true), QStringLiteral("Stopped on this computer"));
        QCOMPARE(why(QStringLiteral("here"), 2, false), QStringLiteral("Stopped on the Core"));
        QCOMPARE(FlagRecordPlay::lengthText(3723000), QStringLiteral("1:02:03"));
    }
};

QTEST_MAIN(TestFlagRecordPlay)
#include "tst_flag_record_play.moc"
