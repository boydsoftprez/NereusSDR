// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_recordings_setup_page.cpp  (NereusSDR)
// =================================================================
//
// Setup > Cores > Recordings (recording design 6.5) against a fake
// recordings access: the groups and the spec's defaults, each control
// sending only its own changed field, a change from another device shown
// without being sent back, the sizes an hour and the name example, the
// disks and the SD card note, Recordings ›, and the page dimmed with the
// reason when not connected or on an older Core. The on-air switches stay
// disabled with the older-Core reason until the Core offers version 2.
// No Core, no sound device, nothing keys.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 8.
//   2026-11-17 - J.J. Boyd (KG4VCF), JJ's request: the VOICE button
//                 switch, this computer's own, on by default, kept in
//                 AppSettings and usable with no Core. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-17 - J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code: the disk drop-down under repeated
//                 Core updates with accessibility on and a stale accessible
//                 cell (PR 808 crash).
// =================================================================

#include <QAbstractItemView>
#include <QAccessible>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include "core/AppSettings.h"
#include "fakes/FakeRecordingsAccess.h"
#include "gui/recordings/VoiceButton.h"
#include "gui/setup/RecordingsSetupPage.h"

using namespace NereusSDR;

namespace {

constexpr qint64 kGb = qint64(1000) * 1000 * 1000;

QJsonObject disk(const QString& root, const QString& name, qint64 free, bool own, bool sd = false)
{
    return QJsonObject{{QStringLiteral("root"), root},   {QStringLiteral("name"), name},
                       {QStringLiteral("free"), free},   {QStringLiteral("total"), free * 2},
                       {QStringLiteral("sd"), sd},       {QStringLiteral("own"), own}};
}

/// The document a Core with the spec's defaults sends, with two disks.
QJsonObject defaultDocument()
{
    QJsonObject settings{{QStringLiteral("keepMyVoice"), true},  {QStringLiteral("sidesApart"), false},
                         {QStringLiteral("autoStart"), false},   {QStringLiteral("autoStop"), true},
                         {QStringLiteral("quietMinutes"), 2},    {QStringLiteral("format"), QStringLiteral("mp3")},
                         {QStringLiteral("disk"), QString()},
                         {QStringLiteral("disks"),
                          QJsonArray{disk(QStringLiteral("/"), QStringLiteral("SSD"), 412 * kGb, true),
                                     disk(QStringLiteral("/Volumes/T7"), QStringLiteral("USB disk: Samsung T7 SSD"),
                                          931 * kGb, false)}}};
    QJsonObject doc = FakeRecordingsAccess::document({}, 1, QString(), settings);
    QJsonObject index = doc.value(QStringLiteral("index")).toObject();
    index.insert(QStringLiteral("count"), 12);
    index.insert(QStringLiteral("usedBytes"), qint64(1400) * 1000 * 1000);
    index.insert(QStringLiteral("freeBytes"), 412 * kGb);
    doc.insert(QStringLiteral("index"), index);
    return doc;
}

QJsonObject withSetting(QJsonObject doc, const QString& field, const QJsonValue& value)
{
    QJsonObject settings = doc.value(QStringLiteral("settings")).toObject();
    settings.insert(field, value);
    doc.insert(QStringLiteral("settings"), settings);
    return doc;
}

}  // namespace

class TestRecordingsSetupPage : public QObject {
    Q_OBJECT

private slots:
    void groupsAndDefaults();
    void eachControlSendsOnlyItsField();
    void documentChangeUpdatesWithoutEcho();
    void sizesAndNameFollowTheFormat();
    void disksAndSdCardNote();
    void recordingsButtonAsksForTheWindow();
    void onAirSwitchesWaitForVersionTwo();
    void notConnectedDimsEveryGroup();
    void olderCoreSaysWhy();
    void voiceButtonSwitchIsThisComputers();
    void coreUpdatesDoNotRebuildTheDisks();
};

void TestRecordingsSetupPage::groupsAndDefaults()
{
    FakeRecordingsAccess access;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);

    QStringList titles;
    for (QGroupBox* g : page.groups()) {
        titles.append(g->title());
        QVERIFY(g->isEnabled());
    }
    QCOMPARE(titles, (QStringList{QStringLiteral("What the Record button keeps"),
                                  QStringLiteral("Starting and stopping"), QStringLiteral("Files"),
                                  QStringLiteral("Where they are kept"), QStringLiteral("Playing back")}));
    QVERIFY(page.availabilityLabel()->isHidden());

    QVERIFY(page.hearBox()->isChecked());
    QVERIFY(!page.hearBox()->isEnabled());
    QVERIFY(!page.hearBox()->toolTip().isEmpty());
    QVERIFY(page.myVoiceBox()->isChecked());
    QCOMPARE(page.sidesCombo()->currentText(), QStringLiteral("Together, one after the other"));
    QVERIFY(page.sidesCombo()->isEnabled());
    QVERIFY(!page.autoStartBox()->isChecked());
    QVERIFY(page.autoStopBox()->isChecked());
    QCOMPARE(page.quietSpin()->value(), 2);
    QCOMPARE(page.quietSpin()->minimum(), 1);
    QCOMPARE(page.quietSpin()->maximum(), 60);
    QCOMPARE(page.formatCombo()->currentText(), QStringLiteral("MP3"));
    QCOMPARE(page.diskCombo()->currentIndex(), 0);
    QCOMPARE(page.playingBackSentence()->text(),
             QStringLiteral("Keying while a recording plays sends it on the air."));
    QStringList switches;
    for (QCheckBox* box : page.onAirBoxes()) {
        switches.append(box->text());
        QVERIFY(!box->isChecked());
    }
    QCOMPARE(switches, (QStringList{QStringLiteral("EQ"), QStringLiteral("Compressor"), QStringLiteral("Leveler"),
                                    QStringLiteral("CFC"), QStringLiteral("Phase rotator")}));
    QCOMPARE(page.usageLabel()->text(),
             QStringLiteral("12 recordings, 1.4 GB. 412 GB free on this disk, room for well over a thousand "
                            "hours more."));
    // Showing the Core's settings sends nothing.
    QVERIFY(access.settingsSent.isEmpty());
}

void TestRecordingsSetupPage::eachControlSendsOnlyItsField()
{
    FakeRecordingsAccess access;
    access.version = 2;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);

    page.myVoiceBox()->setChecked(false);
    page.myVoiceBox()->setChecked(true);
    page.sidesCombo()->setCurrentIndex(1);
    page.autoStartBox()->setChecked(true);
    page.autoStopBox()->setChecked(false);
    page.autoStopBox()->setChecked(true);
    page.quietSpin()->setValue(15);
    page.formatCombo()->setCurrentIndex(1);
    page.diskCombo()->setCurrentIndex(1);
    page.diskCombo()->setCurrentIndex(0);
    for (QCheckBox* box : page.onAirBoxes()) {
        box->setChecked(true);
    }

    const QList<QJsonObject> expected{
        QJsonObject{{QStringLiteral("keepMyVoice"), false}},
        QJsonObject{{QStringLiteral("keepMyVoice"), true}},
        QJsonObject{{QStringLiteral("sidesApart"), true}},
        QJsonObject{{QStringLiteral("autoStart"), true}},
        QJsonObject{{QStringLiteral("autoStop"), false}},
        QJsonObject{{QStringLiteral("autoStop"), true}},
        QJsonObject{{QStringLiteral("quietMinutes"), 15}},
        QJsonObject{{QStringLiteral("format"), QStringLiteral("wav")}},
        QJsonObject{{QStringLiteral("disk"), QStringLiteral("/Volumes/T7")}},
        QJsonObject{{QStringLiteral("disk"), QString()}},
        QJsonObject{{QStringLiteral("onAirUseTxEq"), true}},
        QJsonObject{{QStringLiteral("onAirUseCompressor"), true}},
        QJsonObject{{QStringLiteral("onAirUseLeveler"), true}},
        QJsonObject{{QStringLiteral("onAirUseCfc"), true}},
        QJsonObject{{QStringLiteral("onAirUsePhaseRotator"), true}},
    };
    QCOMPARE(access.settingsSent, expected);
}

void TestRecordingsSetupPage::documentChangeUpdatesWithoutEcho()
{
    FakeRecordingsAccess access;
    access.version = 2;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);

    QJsonObject doc = defaultDocument();
    doc = withSetting(doc, QStringLiteral("keepMyVoice"), false);
    doc = withSetting(doc, QStringLiteral("sidesApart"), true);
    doc = withSetting(doc, QStringLiteral("autoStart"), true);
    doc = withSetting(doc, QStringLiteral("autoStop"), false);
    doc = withSetting(doc, QStringLiteral("quietMinutes"), 30);
    doc = withSetting(doc, QStringLiteral("format"), QStringLiteral("wav"));
    doc = withSetting(doc, QStringLiteral("disk"), QStringLiteral("/Volumes/T7"));
    doc = withSetting(doc, QStringLiteral("onAirUseCfc"), true);
    access.setDocument(doc);

    QVERIFY(!page.myVoiceBox()->isChecked());
    QCOMPARE(page.sidesCombo()->currentIndex(), 1);
    QVERIFY(!page.sidesCombo()->isEnabled());   // follows "your own voice"
    QVERIFY(page.autoStartBox()->isChecked());
    QVERIFY(!page.autoStopBox()->isChecked());
    QCOMPARE(page.quietSpin()->value(), 30);
    QVERIFY(!page.quietSpin()->isEnabled());    // follows "stop by itself"
    QCOMPARE(page.formatCombo()->currentText(), QStringLiteral("WAV"));
    QCOMPARE(page.diskCombo()->currentData().toString(), QStringLiteral("/Volumes/T7"));
    QVERIFY(page.onAirBoxes().at(3)->isChecked());
    QVERIFY(!page.onAirBoxes().at(0)->isChecked());
    QVERIFY(page.startStopSummary()->text().startsWith(
        QStringLiteral("Every contact you answer or call is recorded without pressing Record.")));
    QVERIFY(access.settingsSent.isEmpty());

    // And back to the defaults: still nothing sent.
    access.setDocument(defaultDocument());
    QVERIFY(page.myVoiceBox()->isChecked());
    QCOMPARE(page.startStopSummary()->text(),
             QStringLiteral("Recording starts only when you press Record. A recording left running ends 2 min "
                            "after the band goes quiet."));
    QVERIFY(access.settingsSent.isEmpty());
}

void TestRecordingsSetupPage::sizesAndNameFollowTheFormat()
{
    QCOMPARE(RecordingsSetupPage::megabytesPerHour(false, false), 29);
    QCOMPARE(RecordingsSetupPage::megabytesPerHour(true, false), 173);
    QCOMPARE(RecordingsSetupPage::megabytesPerHour(false, true), 58);
    QCOMPARE(RecordingsSetupPage::megabytesPerHour(true, true), 346);

    FakeRecordingsAccess access;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);
    QCOMPARE(page.formatSizeLabel()->text(), QStringLiteral("Small, plays anywhere. About 29 MB an hour."));
    QCOMPARE(page.nameExample()->text(), QStringLiteral("2026-11-15 1432Z 7.236.400 LSB K4ABC.mp3"));

    access.setDocument(withSetting(defaultDocument(), QStringLiteral("format"), QStringLiteral("wav")));
    QCOMPARE(page.formatSizeLabel()->text(), QStringLiteral("Full quality, bigger. About 173 MB an hour."));
    QCOMPARE(page.nameExample()->text(), QStringLiteral("2026-11-15 1432Z 7.236.400 LSB K4ABC.wav"));
}

void TestRecordingsSetupPage::disksAndSdCardNote()
{
    FakeRecordingsAccess access;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);
    QCOMPARE(page.diskCombo()->count(), 2);
    QCOMPARE(page.diskCombo()->itemText(0), QStringLiteral("The Core's own disk (SSD, 412 GB free)"));
    QCOMPARE(page.diskCombo()->itemText(1), QStringLiteral("USB disk: Samsung T7 SSD (931 GB free)"));
    QVERIFY(page.sdCardNote()->isHidden());

    // A Pi with only an SD card: allowed, with the note.
    QJsonObject doc = withSetting(defaultDocument(), QStringLiteral("disks"),
                                  QJsonArray{disk(QStringLiteral("/"), QStringLiteral("SD card"), 58 * kGb, true,
                                                  true)});
    access.setDocument(doc);
    QCOMPARE(page.diskCombo()->count(), 1);
    QVERIFY(page.diskCombo()->isEnabled());
    QVERIFY(!page.sdCardNote()->isHidden());
    QVERIFY(page.sdCardNote()->text().contains(QStringLiteral("do not wear the card")));
    QVERIFY(access.settingsSent.isEmpty());
}

void TestRecordingsSetupPage::recordingsButtonAsksForTheWindow()
{
    FakeRecordingsAccess access;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);
    QCOMPARE(page.recordingsButton()->text(), QStringLiteral("Recordings ›"));
    QSignalSpy spy(&page, &RecordingsSetupPage::recordingsWindowRequested);
    page.recordingsButton()->click();
    QCOMPARE(spy.count(), 1);
}

void TestRecordingsSetupPage::onAirSwitchesWaitForVersionTwo()
{
    FakeRecordingsAccess access;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);
    for (QCheckBox* box : page.onAirBoxes()) {
        QVERIFY(!box->isHidden());
        QVERIFY(!box->isEnabled());
        QCOMPARE(box->toolTip(), RecordingsAccess::needsNewerCoreReason());
    }
    // The rest of the page works on version 1.
    QVERIFY(page.myVoiceBox()->isEnabled());

    access.version = 2;
    access.setState(RecordingsAccess::State::Ready);
    for (QCheckBox* box : page.onAirBoxes()) {
        QVERIFY(box->isEnabled());
        QVERIFY(box->toolTip().isEmpty());
    }
}

void TestRecordingsSetupPage::notConnectedDimsEveryGroup()
{
    FakeRecordingsAccess access;
    access.setState(RecordingsAccess::State::NotConnected);
    RecordingsSetupPage page(&access);
    QVERIFY(!page.availabilityLabel()->isHidden());
    QCOMPARE(page.availabilityLabel()->text(), RecordingsAccess::notConnectedReason());
    QCOMPARE(page.groups().size(), 5);
    for (QGroupBox* g : page.groups()) {
        QVERIFY(!g->isHidden());
        QVERIFY(!g->isEnabled());
        QCOMPARE(g->toolTip(), RecordingsAccess::notConnectedReason());
    }
    QCOMPARE(page.usageLabel()->text(), QStringLiteral("Not known."));

    // Connecting brings the page back.
    access.setDocument(defaultDocument());
    access.setState(RecordingsAccess::State::Ready);
    QVERIFY(page.availabilityLabel()->isHidden());
    for (QGroupBox* g : page.groups()) {
        QVERIFY(g->isEnabled());
    }
    QVERIFY(access.settingsSent.isEmpty());
}

void TestRecordingsSetupPage::olderCoreSaysWhy()
{
    FakeRecordingsAccess access;
    access.setState(RecordingsAccess::State::NeedsNewerCore);
    RecordingsSetupPage page(&access);
    QCOMPARE(page.availabilityLabel()->text(), QStringLiteral("This Core needs an update for recordings."));
    for (QGroupBox* g : page.groups()) {
        QVERIFY(!g->isHidden());
        QVERIFY(!g->isEnabled());
        QCOMPARE(g->toolTip(), QStringLiteral("This Core needs an update for recordings."));
    }
}

// The VOICE button switch: on by default, written to this computer's
// AppSettings as "True" / "False" and never sent to the Core, usable with
// no Core, and a page opened later shows what was chosen.
void TestRecordingsSetupPage::voiceButtonSwitchIsThisComputers()
{
    AppSettings::instance().remove(VoiceButton::shownSettingKey());
    FakeRecordingsAccess access;
    access.setState(RecordingsAccess::State::NotConnected);
    RecordingsSetupPage page(&access);
    QCheckBox* box = page.voiceButtonBox();
    QVERIFY(box != nullptr);
    QCOMPARE(box->text(), QStringLiteral("Show the VOICE button beside TUNE and MOX"));
    QVERIFY(box->isChecked());
    QVERIFY(box->isEnabled());
    QVERIFY(!page.groups().contains(qobject_cast<QGroupBox*>(box->parentWidget())));

    box->setChecked(false);
    QCOMPARE(AppSettings::instance().value(VoiceButton::shownSettingKey()).toString(), QStringLiteral("False"));
    QVERIFY(access.settingsSent.isEmpty());
    {
        RecordingsSetupPage later(&access);
        QVERIFY(!later.voiceButtonBox()->isChecked());
    }
    box->setChecked(true);
    QCOMPARE(AppSettings::instance().value(VoiceButton::shownSettingKey()).toString(), QStringLiteral("True"));
    QVERIFY(access.settingsSent.isEmpty());
}

// PR 808 crash: the same disks with new free figures change only the words
// in place; other disks reset the list's model instead of removing rows,
// which crashed Qt's accessibility with a stale cell (see
// tst_dsr_setup_page).
void TestRecordingsSetupPage::coreUpdatesDoNotRebuildTheDisks()
{
    // Qt warns when it meets the stale cell planted below, then skips it:
    // expected here, so that category is quiet.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.accessibility.core.warning=false"));
    QAccessible::setActive(true);
    const auto off = qScopeGuard([] {
        QAccessible::setActive(false);
        QLoggingCategory::setFilterRules(QString());
    });
    FakeRecordingsAccess access;
    access.setDocument(defaultDocument());
    RecordingsSetupPage page(&access);
    QComboBox* combo = page.diskCombo();
    QCOMPARE(combo->count(), 2);

    const auto staleCell = [combo] {
        QAccessibleInterface* list = QAccessible::queryAccessibleInterface(combo->view());
        QVERIFY(list != nullptr && list->tableInterface() != nullptr);
        QVERIFY(list->child(0) != nullptr && list->child(1) != nullptr);
        QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(list->child(0)));
    };
    staleCell();
    QSignalSpy removed(combo->model(), &QAbstractItemModel::rowsRemoved);
    QSignalSpy reset(combo->model(), &QAbstractItemModel::modelReset);

    QJsonArray disks =
        defaultDocument().value(QStringLiteral("settings")).toObject().value(QStringLiteral("disks")).toArray();
    for (int n = 1; n <= 3; ++n) {
        disks[0] = disk(QStringLiteral("/"), QStringLiteral("SSD"), (412 - n) * kGb, true);
        access.setDocument(withSetting(defaultDocument(), QStringLiteral("disks"), disks));
        QCOMPARE(combo->itemText(0), QStringLiteral("The Core's own disk (SSD, %1 GB free)").arg(412 - n));
    }
    QCOMPARE(reset.count(), 0);
    QCOMPARE(removed.count(), 0);

    disks.append(disk(QStringLiteral("/Volumes/NVMe"), QStringLiteral("USB disk: NVMe"), 1800 * kGb, false));
    access.setDocument(withSetting(defaultDocument(), QStringLiteral("disks"), disks));
    QCOMPARE(reset.count(), 1);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->currentIndex(), 0);
    QCOMPARE(combo->itemData(2).toString(), QStringLiteral("/Volumes/NVMe"));
    QVERIFY(access.settingsSent.isEmpty());
}

QTEST_MAIN(TestRecordingsSetupPage)
#include "tst_recordings_setup_page.moc"
