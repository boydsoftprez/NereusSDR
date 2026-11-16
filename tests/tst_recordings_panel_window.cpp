// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_recordings_panel_window.cpp  (NereusSDR)
// =================================================================
//
// The Recordings panel and the Recordings window (recording design section
// 3, 6.4 and A4) against a fake recordings access: the panel's six latest,
// its play buttons, red row and right-click menu; the window's totals,
// filter, search, sort, paging, rename, delete several with its confirm,
// copies with progress, Show in Finder and Save sound only; a rename in the
// window reaching the panel and a flag's list; both dimmed with the reason
// when recordings cannot be used. No Core, no sound device, nothing keys.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 7.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: a whole-pan
//                 row's reason is now the older Core's. AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>
#include <QTimeZone>

#include <memory>

#include "fakes/FakeRecordingsAccess.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/recordings/FlagRecordPlay.h"
#include "gui/recordings/RecordingsPanel.h"
#include "gui/recordings/RecordingsWindow.h"
#include "gui/recordings/RevealFile.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

const QDateTime kNow(QDate(2026, 11, 15), QTime(15, 2), QTimeZone::utc());
// A whole-pan stretch needs a Core with the recorder (recordingsVersion 4).
const QString kDsrReason = QStringLiteral("This Core needs an update for recordings.");

RecordingEntry entry(const QString& id, RecordingKind kind, int slice, qint64 agoSeconds, qint64 lengthMs,
                     qint64 bytes, qint64 hz = 7236400, const QString& call = QString())
{
    RecordingEntry e;
    e.id = id;
    e.kind = kind;
    e.sliceId = slice;
    e.startUtc = kNow.addSecs(-agoSeconds);
    e.lengthMs = lengthMs;
    e.frequencyHz = hz;
    e.mode = kind == RecordingKind::WholePan ? QString() : QStringLiteral("LSB");
    e.call = call;
    e.name = recordingBaseName(e.startUtc, hz, e.mode, call);
    e.fileName = e.name + QStringLiteral(".mp3");
    e.sizeBytes = bytes;
    return e;
}

// Newest first, as the Core keeps them.
QList<RecordingEntry> nine()
{
    return {entry(QStringLiteral("c1"), RecordingKind::Contact, 0, 1800, 312000, 9000000, 7185000,
                  QStringLiteral("K4ABC")),
            entry(QStringLiteral("w1"), RecordingKind::WholePan, -1, 5400, 600000, 1640000000, 7220000),
            entry(QStringLiteral("c2"), RecordingKind::Contact, 1, 9000, 205000, 6000000, 7236400,
                  QStringLiteral("W1XYZ")),
            entry(QStringLiteral("s1"), RecordingKind::Sound, -1, 86400, 64000, 2000000, 14250000),
            entry(QStringLiteral("c3"), RecordingKind::Contact, 0, 90000, 540000, 15000000, 7200000,
                  QStringLiteral("N4QQ")),
            entry(QStringLiteral("c4"), RecordingKind::Contact, 0, 170000, 1210000, 34000000, 3880000),
            entry(QStringLiteral("c5"), RecordingKind::Contact, 2, 200000, 188000, 5000000, 14313000,
                  QStringLiteral("VE3ABC")),
            entry(QStringLiteral("s2"), RecordingKind::Sound, -1, 260000, 402000, 11000000, 21300000,
                  QStringLiteral("EA8XX")),
            entry(QStringLiteral("c6"), RecordingKind::Contact, 0, 400000, 150000, 4000000, 7191000,
                  QStringLiteral("KD4Z"))};
}

QJsonObject documentFor(const QList<RecordingEntry>& entries, const QJsonArray& slices = {}, qint64 revision = 1)
{
    QJsonObject doc = FakeRecordingsAccess::document(slices, revision, QStringLiteral("/Recordings"));
    qint64 used = 0;
    for (const RecordingEntry& e : entries) {
        used += e.sizeBytes;
    }
    QJsonObject index = doc.value(QStringLiteral("index")).toObject();
    index.insert(QStringLiteral("count"), entries.size());
    index.insert(QStringLiteral("usedBytes"), used);
    index.insert(QStringLiteral("freeBytes"), qint64(884) * 1000 * 1000 * 1000);
    doc.insert(QStringLiteral("index"), index);
    QJsonObject settings = doc.value(QStringLiteral("settings")).toObject();
    settings.insert(QStringLiteral("disk"), QString());
    settings.insert(QStringLiteral("disks"),
                    QJsonArray{QJsonObject{{QStringLiteral("root"), QStringLiteral("/")},
                                           {QStringLiteral("name"), QStringLiteral("USB SSD")},
                                           {QStringLiteral("own"), true}}});
    doc.insert(QStringLiteral("settings"), settings);
    return doc;
}

struct Fixture {
    FakeRecordingsAccess access;
    explicit Fixture(const QList<RecordingEntry>& entries = nine(), const QJsonArray& slices = {})
    {
        access.entries = entries;
        access.setDocument(documentFor(entries, slices));
    }
};

QStringList actionTexts(QMenu& menu)
{
    QStringList out;
    for (QAction* a : menu.actions()) {
        out.append(a->isSeparator() ? QStringLiteral("-") : a->text().section(QLatin1Char('\t'), 0, 0));
    }
    return out;
}

QAction* actionNamed(QMenu& menu, const QString& text)
{
    for (QAction* a : menu.actions()) {
        if (a->text().section(QLatin1Char('\t'), 0, 0) == text) {
            return a;
        }
    }
    return nullptr;
}

QList<FakeRecordingsAccess::Call> callsOf(const FakeRecordingsAccess& access, const QString& verb)
{
    QList<FakeRecordingsAccess::Call> out;
    for (const FakeRecordingsAccess::Call& c : access.calls) {
        if (c.verb == verb) {
            out.append(c);
        }
    }
    return out;
}

}  // namespace

class TstRecordingsPanelWindow : public QObject {
    Q_OBJECT

private slots:
    // ── The panel ────────────────────────────────────────────────────────

    void the_panel_shows_the_six_latest_each_with_play()
    {
        Fixture f;
        RecordingsPanel panel(nullptr, &f.access);
        panel.setClockForTest([] { return kNow; });
        QCOMPARE(panel.appletTitle(), QStringLiteral("Recordings"));
        QCOMPARE(panel.rowIds(), (QStringList{QStringLiteral("c1"), QStringLiteral("w1"), QStringLiteral("c2"),
                                              QStringLiteral("s1"), QStringLiteral("c3"), QStringLiteral("c4")}));
        const FakeRecordingsAccess::Call asked = callsOf(f.access, QStringLiteral("requestList")).last();
        QCOMPARE(asked.count, RecordingsPanel::kRows);
        QVERIFY(!asked.listSlice.has_value());
        QVERIFY(!asked.listKind.has_value());
        for (const QString& id : panel.rowIds()) {
            QVERIFY(panel.rowPlayButtonForTest(id) != nullptr);
        }
        QCOMPARE(panel.rowTitleForTest(QStringLiteral("c1"))->text(), QStringLiteral("7.185.000 LSB  K4ABC"));
        QCOMPARE(panel.rowLengthForTest(QStringLiteral("c1"))->text(), QStringLiteral("5:12"));
        QVERIFY(panel.usageForTest()->text().contains(QStringLiteral("9 on the Core, 1.7 GB")));
        QVERIFY(panel.usageForTest()->text().contains(QStringLiteral("884 GB free on USB SSD")));
        QVERIFY(panel.noteForTest()->isHidden());

        // A whole-pan stretch opens on the pan with a Core that has the recorder.
        QPushButton* pan = panel.rowPlayButtonForTest(QStringLiteral("w1"));
        QVERIFY(!pan->isEnabled());
        QCOMPARE(pan->toolTip(), kDsrReason);

        f.access.calls.clear();
        panel.rowPlayButtonForTest(QStringLiteral("c2"))->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("play")});
        QCOMPARE(f.access.calls.last().id, QStringLiteral("c2"));

        f.access.setPlayback(PlaybackState{QStringLiteral("local"), QStringLiteral("c2"), 12000, 205000, true, false});
        QCOMPARE(panel.rowForTest(QStringLiteral("c2"))->property("look").toString(), QStringLiteral("playing"));
        QCOMPARE(panel.rowLengthForTest(QStringLiteral("c2"))->text(), QStringLiteral("0:12 / 3:25"));
        QVERIFY(!panel.rowProgressForTest(QStringLiteral("c2"))->isHidden());
        QCOMPARE(panel.rowProgressForTest(QStringLiteral("c2"))->value(), 12000 * 1000 / 205000);
        f.access.calls.clear();
        panel.rowPlayButtonForTest(QStringLiteral("c2"))->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("stopPlay")});

        QSignalSpy all(&panel, &RecordingsPanel::allRecordingsRequested);
        panel.allButtonForTest()->click();
        QCOMPARE(all.count(), 1);
        QCOMPARE(all.first().at(0).toString(), QString());
    }

    void the_panel_shows_a_red_row_while_a_slice_records()
    {
        Fixture f(nine(), {FakeRecordingsAccess::slice(0, QStringLiteral("recording"), 83000, true)});
        RecordingsPanel panel(nullptr, &f.access);
        panel.setSliceTextSource([](int slice) { return slice == 0 ? QStringLiteral("7.185.000 LSB") : QString(); });
        panel.syncFromModel();
        QCOMPARE(panel.rowIds().size(), RecordingsPanel::kRows);
        QCOMPARE(panel.rowIds().first(), QStringLiteral("slice:0"));
        QCOMPARE(panel.rowIds().at(1), QStringLiteral("c1"));
        QCOMPARE(panel.rowForTest(QStringLiteral("slice:0"))->property("look").toString(), QStringLiteral("recording"));
        QVERIFY(panel.rowTitleForTest(QStringLiteral("slice:0"))->text().contains(QStringLiteral("Recording Slice A")));
        QCOMPARE(panel.rowLengthForTest(QStringLiteral("slice:0"))->text(), QStringLiteral("1:23"));
        f.access.calls.clear();
        panel.rowPlayButtonForTest(QStringLiteral("slice:0"))->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("stop")});
        QCOMPARE(f.access.calls.last().sliceId, 0);
    }

    void the_panel_menu_plays_renames_copies_shows_and_deletes()
    {
        Fixture f;
        f.access.coreHere = false;
        RecordingsPanel panel(nullptr, &f.access);
        {
            QMenu menu;
            panel.populateRowMenu(&menu, QStringLiteral("c1"));
            QCOMPARE(actionTexts(menu), (QStringList{QStringLiteral("Play"), QStringLiteral("Rename…"),
                                                     QStringLiteral("Save a copy here…"),
                                                     RevealFile::actionText(), QStringLiteral("-"),
                                                     QStringLiteral("Delete…")}));
            QAction* show = actionNamed(menu, RevealFile::actionText());
            QVERIFY(!show->isEnabled());
            QCOMPARE(show->toolTip(), QStringLiteral("The Core is on another computer."));
            f.access.calls.clear();
            actionNamed(menu, QStringLiteral("Play"))->trigger();
            QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("play")});
        }
        f.access.coreHere = true;
        QStringList revealed;
        panel.setRevealForTest([&revealed](const QString& path) {
            revealed.append(path);
            return true;
        });
        {
            QMenu menu;
            panel.populateRowMenu(&menu, QStringLiteral("c1"));
            QAction* show = actionNamed(menu, RevealFile::actionText());
            QVERIFY(show->isEnabled());
            show->trigger();
            QCOMPARE(revealed.size(), 1);
            QVERIFY(revealed.first().endsWith(nine().first().fileName));
        }

        // Save a copy here: a file dialog, then the copy.
        panel.setFileChooserForTest([](const QString& suggested) {
            return suggested.endsWith(QStringLiteral(".mp3")) ? QStringLiteral("/tmp/copy.mp3") : QString();
        });
        {
            QMenu menu;
            panel.populateRowMenu(&menu, QStringLiteral("c1"));
            f.access.calls.clear();
            actionNamed(menu, QStringLiteral("Save a copy here…"))->trigger();
            QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("download")});
            QCOMPARE(f.access.calls.last().id, QStringLiteral("c1>/tmp/copy.mp3"));
            emit f.access.downloadProgress(QStringLiteral("c1"), 50, 100);
            QVERIFY(panel.usageForTest()->text().contains(QStringLiteral("Copying, 50%")));
            emit f.access.downloadFinished(QStringLiteral("c1"), true, QString());
            QVERIFY(!panel.usageForTest()->text().contains(QStringLiteral("Copying")));
        }

        // Delete asks first and says it cannot be undone.
        QStringList asked;
        bool answer = false;
        panel.setConfirmForTest([&asked, &answer](RecordingsDeleteConfirm& confirm) {
            asked.append(confirm.text());
            return answer;
        });
        for (const bool yes : {false, true}) {
            answer = yes;
            QMenu menu;
            panel.populateRowMenu(&menu, QStringLiteral("c3"));
            f.access.calls.clear();
            actionNamed(menu, QStringLiteral("Delete…"))->trigger();
            QCOMPARE(f.access.verbs(), yes ? QStringList{QStringLiteral("remove")} : QStringList());
        }
        QCOMPARE(asked.size(), 2);
        QVERIFY(asked.first().contains(QStringLiteral("cannot be undone")));
        QVERIFY(asked.first().contains(QStringLiteral("7.200.000 LSB  N4QQ")));
        QCOMPARE(f.access.calls.last().id, QStringLiteral("c3"));

        // Rename in place: Escape keeps the name, Enter sends the new one.
        panel.startRename(QStringLiteral("c4"));
        QVERIFY(panel.renameEditorForTest() != nullptr);
        QTest::keyClick(panel.renameEditorForTest(), Qt::Key_Escape);
        QTRY_VERIFY(panel.renameEditorForTest() == nullptr);
        QVERIFY(callsOf(f.access, QStringLiteral("rename")).isEmpty());
        panel.startRename(QStringLiteral("c4"));
        panel.renameEditorForTest()->setText(QStringLiteral("80 m net"));
        QTest::keyClick(panel.renameEditorForTest(), Qt::Key_Return);
        QCOMPARE(callsOf(f.access, QStringLiteral("rename")).last().id, QStringLiteral("c4=80 m net"));
    }

    void the_panel_stays_dimmed_with_the_reason()
    {
        Fixture f;
        RecordingsPanel panel(nullptr, &f.access);
        f.access.setState(RecordingsAccess::State::NeedsNewerCore);
        QVERIFY(!panel.noteForTest()->isHidden());
        QCOMPARE(panel.noteForTest()->text(), QStringLiteral("This Core needs an update for recordings."));
        QVERIFY(!panel.allButtonForTest()->isEnabled());
        QVERIFY(panel.rowIds().isEmpty());
        f.access.setState(RecordingsAccess::State::NotConnected);
        QCOMPARE(panel.noteForTest()->text(), QStringLiteral("Connect to a radio to use recordings."));
        f.access.setState(RecordingsAccess::State::Ready);
        QVERIFY(panel.noteForTest()->isHidden());
        QVERIFY(panel.allButtonForTest()->isEnabled());
        QCOMPARE(panel.rowIds().size(), RecordingsPanel::kRows);

        RecordingsPanel none(nullptr, nullptr);
        QCOMPARE(none.noteForTest()->text(), QStringLiteral("Connect to a radio to use recordings."));
        QVERIFY(!none.allButtonForTest()->isEnabled());
    }

    void the_panel_is_listed_under_containers_applets_and_starts_hidden()
    {
        QCOMPARE(ContainerContentRegistry::appletTypeForVisibilityId(QStringLiteral("Recordings")),
                 QStringLiteral("applet:recordings"));
        ContainerContentRegistry registry;
        bool listed = false;
        for (const auto& d : registry.descriptors()) {
            if (d.typeId == QLatin1String("applet:recordings")) {
                listed = true;
                QCOMPARE(d.title, QStringLiteral("Recordings"));
                QVERIFY(d.group == ContentGroup::Panels);
                QVERIFY(d.singleton);
            }
        }
        QVERIFY(listed);
        QVERIFY(!ContainerContentRegistry::firstLaunchEntry(registry.makeEntry(QStringLiteral("applet:recordings")))
                     .visible);
    }

    // ── The window ───────────────────────────────────────────────────────

    void the_window_totals_filter_search_and_sort()
    {
        Fixture f;
        RecordingsWindow window(&f.access);
        QCOMPARE(window.windowTitle(), QStringLiteral("Recordings"));
        QCOMPARE(window.entries().size(), 9);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("c1"));
        QCOMPARE(window.totalsForTest()->text(), QStringLiteral("<b>1.7 GB</b> saved in 9 recordings"));
        QCOMPARE(window.totalsDetailForTest()->text(),
                 QStringLiteral("Whole pan 1.6 GB, sound 86 MB. 884 GB free on USB SSD. The Digital Spectrum "
                                "Recorder's loop is separate and not counted here."));
        QCOMPARE(window.tableForTest()->columnCount(), int(RecordingsWindow::ColCount));
        QStringList headers;
        for (int c = 1; c < RecordingsWindow::ColCount; ++c) {
            headers.append(window.tableForTest()->horizontalHeaderItem(c)->text());
        }
        QCOMPARE(headers, (QStringList{QStringLiteral("When (UTC)"), QStringLiteral("Kind"),
                                       QStringLiteral("Frequency"), QStringLiteral("Mode"), QStringLiteral("Call"),
                                       QStringLiteral("Name"), QStringLiteral("Length"), QStringLiteral("Size")}));

        window.filterButtonForTest(RecordingsWindow::Filter::WholePan)->click();
        QCOMPARE(window.visibleIds(), QStringList{QStringLiteral("w1")});
        window.filterButtonForTest(RecordingsWindow::Filter::Sound)->click();
        QCOMPARE(window.visibleIds(), (QStringList{QStringLiteral("s1"), QStringLiteral("s2")}));
        window.filterButtonForTest(RecordingsWindow::Filter::Contact)->click();
        QCOMPARE(window.visibleIds().size(), 6);
        window.filterButtonForTest(RecordingsWindow::Filter::All)->click();
        QCOMPARE(window.visibleIds().size(), 9);

        // Search covers the call and the name.
        QTest::keyClicks(window.searchForTest(), QStringLiteral("ve3"));
        QCOMPARE(window.visibleIds(), QStringList{QStringLiteral("c5")});
        window.setSearchText(QStringLiteral("14.250"));
        QCOMPARE(window.visibleIds(), QStringList{QStringLiteral("s1")});
        window.setSearchText(QString());

        // A click on a column sorts by it; again turns it round.
        QHeaderView* header = window.tableForTest()->horizontalHeader();
        emit header->sectionClicked(RecordingsWindow::ColSize);
        QCOMPARE(window.sortOrder(), Qt::DescendingOrder);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("w1"));
        emit header->sectionClicked(RecordingsWindow::ColSize);
        QCOMPARE(window.sortOrder(), Qt::AscendingOrder);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("s1"));
        emit header->sectionClicked(RecordingsWindow::ColCall);
        QCOMPARE(window.sortOrder(), Qt::AscendingOrder);
        // No call first, then EA8XX.
        QCOMPARE(window.visibleIds().at(3), QStringLiteral("s2"));
        window.sortBy(RecordingsWindow::ColFrequency, Qt::AscendingOrder);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("c4"));
        window.sortBy(RecordingsWindow::ColLength, Qt::DescendingOrder);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("c4"));
        window.sortBy(RecordingsWindow::ColWhen, Qt::AscendingOrder);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("c6"));
        f.access.setPlayback(PlaybackState{QStringLiteral("local"), QStringLiteral("c3"), 1000, 540000, true, false});
        window.sortBy(RecordingsWindow::ColPlay, Qt::DescendingOrder);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("c3"));
    }

    void the_window_pages_the_whole_list_and_follows_the_index()
    {
        QList<RecordingEntry> many;
        for (int i = 0; i < 230; ++i) {
            many.append(entry(QStringLiteral("r%1").arg(i), RecordingKind::Contact, 0, 60 * i, 1000, 1000));
        }
        Fixture f(many);
        f.access.calls.clear();
        RecordingsWindow window(&f.access);
        const QList<FakeRecordingsAccess::Call> pages = callsOf(f.access, QStringLiteral("requestList"));
        QCOMPARE(pages.size(), 3);
        QCOMPARE(pages.at(0).offset, 0);
        QCOMPARE(pages.at(1).offset, 100);
        QCOMPARE(pages.at(2).offset, 200);
        QCOMPARE(pages.at(0).count, RecordingsWindow::kPageSize);
        QCOMPARE(window.entries().size(), 230);
        QCOMPARE(window.tableForTest()->rowCount(), 230);

        f.access.appliesChanges = true;
        f.access.remove({QStringLiteral("r0")});
        QCOMPARE(window.entries().size(), 229);
        QCOMPARE(window.visibleIds().first(), QStringLiteral("r1"));
    }

    void a_rename_in_the_window_shows_in_the_panel_and_the_flag()
    {
        Fixture f;
        f.access.appliesChanges = true;
        RecordingsWindow window(&f.access);
        RecordingsPanel panel(nullptr, &f.access);
        QWidget pan;
        pan.resize(900, 500);
        auto* flag = new VfoWidget(&pan);
        flag->setSliceIndex(0);
        FlagRecordPlay rp(flag, &f.access, 0);
        pan.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pan));

        window.setSelectedIds({QStringLiteral("c1")});
        QVERIFY(window.renameButtonForTest()->isEnabled());
        window.setRenamePromptForTest([](const QString& current) {
            return current.endsWith(QStringLiteral("K4ABC")) ? std::optional<QString>(QStringLiteral("Field Day"))
                                                             : std::nullopt;
        });
        window.renameButtonForTest()->click();
        QCOMPARE(callsOf(f.access, QStringLiteral("rename")).last().id, QStringLiteral("c1=Field Day"));
        QCOMPARE(window.entries().first().name, QStringLiteral("Field Day"));
        QCOMPARE(panel.rowTitleForTest(QStringLiteral("c1"))->text(), QStringLiteral("Field Day"));
        QCOMPARE(rp.recentRecordings().first().name, QStringLiteral("Field Day"));
        // Each asked with its own reader.
        QSet<int> readers;
        for (const FakeRecordingsAccess::Call& c : callsOf(f.access, QStringLiteral("requestList"))) {
            readers.insert(c.reader);
        }
        QCOMPARE(readers.size(), 3);
        // The selection stays on the renamed one.
        QCOMPARE(window.selectedIds(), QStringList{QStringLiteral("c1")});

        // A name the Core would refuse is refused here, with the reason.
        window.setRenamePromptForTest([](const QString&) { return std::optional<QString>(QStringLiteral("a/b")); });
        f.access.calls.clear();
        window.renameButtonForTest()->click();
        QVERIFY(callsOf(f.access, QStringLiteral("rename")).isEmpty());
        QVERIFY(!window.statusForTest()->text().isEmpty());
    }

    void the_window_deletes_several_after_a_confirm()
    {
        Fixture f;
        RecordingsWindow window(&f.access);
        window.setSelectedIds({QStringLiteral("c2"), QStringLiteral("s1"), QStringLiteral("c3")});
        QCOMPARE(window.deleteButtonForTest()->text(), QStringLiteral("Delete 3…"));
        QStringList asked;
        bool answer = false;
        QStringList buttons;
        window.setConfirmForTest([&asked, &answer, &buttons](RecordingsDeleteConfirm& confirm) {
            asked.append(confirm.text());
            buttons = {confirm.cancelButton()->text(), confirm.deleteButton()->text()};
            return answer;
        });
        window.deleteButtonForTest()->click();
        QVERIFY(callsOf(f.access, QStringLiteral("remove")).isEmpty());
        answer = true;
        window.deleteButtonForTest()->click();
        QCOMPARE(callsOf(f.access, QStringLiteral("remove")).size(), 1);
        QCOMPARE(callsOf(f.access, QStringLiteral("remove")).first().id, QStringLiteral("c2,s1,c3"));
        QCOMPARE(asked.size(), 2);
        QCOMPARE(buttons, (QStringList{QStringLiteral("Cancel"), QStringLiteral("Delete")}));
        QVERIFY(asked.first().contains(QStringLiteral("Delete 3 recordings from the Core?")));
        QVERIFY(asked.first().contains(QStringLiteral("cannot be undone")));
        QVERIFY(asked.first().contains(QStringLiteral("This frees 23 MB")));

        window.setSelectedIds({QStringLiteral("c2")});
        QCOMPARE(window.deleteButtonForTest()->text(), QStringLiteral("Delete…"));
        window.setSelectedIds({});
        QVERIFY(!window.deleteButtonForTest()->isEnabled());
    }

    void the_window_saves_copies_with_progress()
    {
        Fixture f;
        RecordingsWindow window(&f.access);
        window.setSelectedIds({QStringLiteral("c1"), QStringLiteral("c2")});
        QVERIFY(window.copyButtonForTest()->isEnabled());
        window.setFileChooserForTest([](const QString&, bool folder) {
            return folder ? QStringLiteral("/tmp/copies") : QString();
        });
        QVERIFY(window.copyProgressForTest()->isHidden());
        window.copyButtonForTest()->click();
        QCOMPARE(callsOf(f.access, QStringLiteral("download")).size(), 1);
        QCOMPARE(callsOf(f.access, QStringLiteral("download")).first().id,
                 QStringLiteral("c1>/tmp/copies/") + nine().at(0).fileName);
        QVERIFY(!window.copyProgressForTest()->isHidden());
        QVERIFY(window.statusForTest()->text().startsWith(QStringLiteral("Copying 1 of 2")));
        emit f.access.downloadProgress(QStringLiteral("c1"), 3, 4);
        QCOMPARE(window.copyProgressForTest()->value(), 750);
        emit f.access.downloadFinished(QStringLiteral("c1"), true, QString());
        QCOMPARE(callsOf(f.access, QStringLiteral("download")).size(), 2);
        QCOMPARE(callsOf(f.access, QStringLiteral("download")).last().id,
                 QStringLiteral("c2>/tmp/copies/") + nine().at(2).fileName);
        QVERIFY(window.statusForTest()->text().startsWith(QStringLiteral("Copying 2 of 2")));
        emit f.access.downloadFinished(QStringLiteral("c2"), false, QStringLiteral("The connection to the Core was lost."));
        QVERIFY(window.copyProgressForTest()->isHidden());
        QVERIFY(window.statusForTest()->text().contains(QStringLiteral("The connection to the Core was lost.")));

        // One recording: a save dialog with its own name.
        window.setSelectedIds({QStringLiteral("s1")});
        QString suggested;
        window.setFileChooserForTest([&suggested](const QString& path, bool folder) {
            suggested = path;
            return folder ? QString() : QStringLiteral("/tmp/one.mp3");
        });
        window.copyButtonForTest()->click();
        QVERIFY(suggested.endsWith(nine().at(3).fileName));
        QCOMPARE(callsOf(f.access, QStringLiteral("download")).last().id, QStringLiteral("s1>/tmp/one.mp3"));
    }

    void the_window_buttons_follow_the_selection()
    {
        Fixture f;
        f.access.coreHere = false;
        RecordingsWindow window(&f.access);
        for (QPushButton* b : {window.playButtonForTest(), window.renameButtonForTest(), window.soundOnlyButtonForTest(),
                               window.copyButtonForTest(), window.revealButtonForTest(), window.deleteButtonForTest()}) {
            QVERIFY(!b->isEnabled());
        }
        window.setSelectedIds({QStringLiteral("c1")});
        QVERIFY(window.playButtonForTest()->isEnabled());
        QVERIFY(!window.soundOnlyButtonForTest()->isEnabled());
        QVERIFY(!window.revealButtonForTest()->isEnabled());
        QCOMPARE(window.revealButtonForTest()->toolTip(),
                 QStringLiteral("The Core is on another computer. Use Save a copy here."));
        f.access.calls.clear();
        window.playButtonForTest()->click();
        QCOMPARE(f.access.verbs(), QStringList{QStringLiteral("play")});
        f.access.setPlayback(PlaybackState{QStringLiteral("local"), QStringLiteral("c1"), 0, 312000, true, false});
        QCOMPARE(window.playButtonForTest()->text(), QStringLiteral("Stop"));
        window.playButtonForTest()->click();
        QCOMPARE(f.access.verbs().last(), QStringLiteral("stopPlay"));

        // Save sound only is the whole pan's, and needs a Core with the recorder.
        window.setSelectedIds({QStringLiteral("w1")});
        QVERIFY(!window.soundOnlyButtonForTest()->isEnabled());
        QCOMPARE(window.soundOnlyButtonForTest()->toolTip(), kDsrReason);
        QVERIFY(!window.playButtonForTest()->isEnabled());
        QCOMPARE(window.playButtonForTest()->toolTip(), kDsrReason);

        f.access.coreHere = true;
        QStringList revealed;
        window.setRevealForTest([&revealed](const QString& path) {
            revealed.append(path);
            return true;
        });
        window.setSelectedIds({QStringLiteral("c2")});
        QVERIFY(window.revealButtonForTest()->isEnabled());
        window.revealButtonForTest()->click();
        QCOMPARE(revealed.size(), 1);
        QVERIFY(revealed.first().endsWith(nine().at(2).fileName));

        // All recordings... from a flag's Kept notice selects that one.
        window.selectRecording(QStringLiteral("s2"));
        QCOMPARE(window.selectedIds(), QStringList{QStringLiteral("s2")});
    }

    void the_window_stays_dimmed_with_the_reason()
    {
        Fixture f;
        RecordingsWindow window(&f.access);
        QVERIFY(window.noteForTest()->isHidden());
        f.access.setState(RecordingsAccess::State::NeedsNewerCore);
        QVERIFY(!window.noteForTest()->isHidden());
        QCOMPARE(window.noteForTest()->text(), QStringLiteral("This Core needs an update for recordings."));
        QVERIFY(!window.tableForTest()->isEnabled());
        QVERIFY(!window.copyButtonForTest()->isEnabled());
        f.access.setState(RecordingsAccess::State::NotConnected);
        QCOMPARE(window.noteForTest()->text(), QStringLiteral("Connect to a radio to use recordings."));
        f.access.setState(RecordingsAccess::State::Ready);
        QVERIFY(window.noteForTest()->isHidden());
        QCOMPARE(window.entries().size(), 9);
    }

    void show_for_reuses_the_window_under_its_parent()
    {
        Fixture f;
        QWidget parent;
        RecordingsWindow* first = RecordingsWindow::showFor(&f.access, &parent);
        RecordingsWindow* second = RecordingsWindow::showFor(&f.access, &parent);
        QCOMPARE(first, second);
        QVERIFY(first->isVisible());
        QVERIFY(first->isWindow());
        first->hide();
    }
};

QTEST_MAIN(TstRecordingsPanelWindow)
#include "tst_recordings_panel_window.moc"
