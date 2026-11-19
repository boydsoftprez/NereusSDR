// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_dsr_setup_page.cpp  (NereusSDR)
// =================================================================
//
// Setup > Cores > Digital Spectrum Recorder (recording design 3 "Recorder
// Setup", 6.7, A14) against a fake recordings access: on and its status,
// the disks, the size limit and the hours it holds at the receivers and
// rates streaming now, the "It is always erasing" note, the in-use bar with
// its moving head and red zone, saved stretches, each control sending only
// its own field, a change from another device shown without being sent
// back, the erase asking first, an SD card only Core with the switch off
// and disabled and the red warning, and the page dimmed with the reason
// when not connected or on an older Core. No Core, nothing keys.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 26.
//   2026-11-17 - J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code: the disk drop-down under repeated
//                 Core updates with accessibility on, a stale accessible
//                 cell and its list open (PR 808 crash).
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
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTest>
#include <QTimeZone>

#include <cmath>

#include "core/recordings/dsr/DsrRingFile.h"
#include "fakes/FakeRecordingsAccess.h"
#include "gui/setup/DsrSetupPage.h"

using namespace NereusSDR;

namespace {

constexpr qint64 kGb = qint64(1000) * 1000 * 1000;
constexpr int kRate = 192000;
// Two receivers at 192 kHz, one block a second each, as the Core works it.
const qint64 kTwoAt192PerHour = DsrRingFile::blockBytes(kRate) * 3600 * 2;

QJsonObject disk(const QString& root, const QString& name, qint64 free, bool own, bool sd = false)
{
    return QJsonObject{{QStringLiteral("root"), root},   {QStringLiteral("name"), name},
                       {QStringLiteral("free"), free},   {QStringLiteral("total"), free * 2},
                       {QStringLiteral("sd"), sd},       {QStringLiteral("own"), own}};
}

QJsonArray twoReceivers()
{
    return QJsonArray{QJsonObject{{QStringLiteral("receiver"), 0}, {QStringLiteral("rateHz"), kRate}},
                      QJsonObject{{QStringLiteral("receiver"), 1}, {QStringLiteral("rateHz"), kRate}}};
}

/// The `dsr` part of a Core recording two receivers into a full 10 GB loop
/// on its own SSD.
QJsonObject fullDsr()
{
    return QJsonObject{{QStringLiteral("enabled"), true},
                       {QStringLiteral("disk"), QString()},
                       {QStringLiteral("sizeLimitGb"), 10},
                       {QStringLiteral("recording"), true},
                       {QStringLiteral("reason"), QString()},
                       {QStringLiteral("sizeLimitBytes"), 10 * kGb},
                       {QStringLiteral("usedBytes"), 10 * kGb - 300000},
                       {QStringLiteral("writePosition"), 4 * kGb},
                       {QStringLiteral("oldestUtcMs"), qint64(1794660120000)},   // 2026-11-14 12:42 UTC
                       {QStringLiteral("newestUtcMs"), qint64(1794666720000)},   // 14:32 UTC
                       {QStringLiteral("hoursHeld"), 1.83},
                       {QStringLiteral("overflows"), 0},
                       {QStringLiteral("rewound"), QJsonArray()},
                       {QStringLiteral("stretch"), QJsonValue()},
                       {QStringLiteral("saving"), 0},
                       {QStringLiteral("receivers"), twoReceivers()},
                       {QStringLiteral("bytesPerHour"), kTwoAt192PerHour}};
}

QJsonObject document(const QJsonObject& dsr, const QJsonArray& disks)
{
    QJsonObject settings{{QStringLiteral("format"), QStringLiteral("mp3")},
                         {QStringLiteral("disk"), QString()},
                         {QStringLiteral("disks"), disks}};
    QJsonObject doc = FakeRecordingsAccess::document({}, 1, QString(), settings);
    doc.insert(QStringLiteral("version"), 4);
    doc.insert(QStringLiteral("dsr"), dsr);
    return doc;
}

QJsonArray ssdDisks()
{
    return QJsonArray{disk(QStringLiteral("/"), QStringLiteral("SSD"), 412 * kGb, true),
                      disk(QStringLiteral("/Volumes/T7"), QStringLiteral("USB disk: Samsung T7 SSD"), 931 * kGb,
                           false)};
}

QJsonArray sdOnlyDisks()
{
    return QJsonArray{disk(QStringLiteral("/"), QStringLiteral("SD card"), 58 * kGb, true, true)};
}

QJsonObject with(QJsonObject object, const QString& field, const QJsonValue& value)
{
    object.insert(field, value);
    return object;
}

RecordingEntry saved(const QString& id, RecordingKind kind, qint64 bytes)
{
    RecordingEntry e;
    e.id = id;
    e.kind = kind;
    e.startUtc = QDateTime(QDate(2026, 11, 14), QTime(14, 0), QTimeZone::utc());
    e.lengthMs = 600000;
    e.sizeBytes = bytes;
    e.name = id;
    return e;
}

/// The Core's figures a second later: more written, a little less free.
QJsonObject secondLater(int n)
{
    QJsonObject dsr = fullDsr();
    dsr.insert(QStringLiteral("writePosition"), 4 * kGb + n * 1000 * 1000);
    QJsonArray disks = ssdDisks();
    disks[0] = disk(QStringLiteral("/"), QStringLiteral("SSD"), (412 - n) * kGb, true);
    return document(dsr, disks);
}

/// Qt accessibility on for one test, as on JJ's Mac with screen-control
/// tools running.
/// Qt warns "Cannot create accessible child interface" when it meets the
/// stale cell leaveAStaleCell() plants, then skips it; that is expected
/// here, so its category is quiet while on.
struct AccessibilityOn {
    AccessibilityOn()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("qt.accessibility.core.warning=false"));
        QAccessible::setActive(true);
    }
    ~AccessibilityOn()
    {
        QAccessible::setActive(false);
        QLoggingCategory::setFilterRules(QString());
    }
    AccessibilityOn(const AccessibilityOn&) = delete;
    AccessibilityOn& operator=(const AccessibilityOn&) = delete;
};

/// Builds the accessible cells of the drop-down's list, as a screen reader
/// does, then deletes the first cell's interface behind the list's back:
/// the state JJ's crash found (QAccessibleTable::modelChange calling
/// role() on a cached cell whose interface is gone). QComboBox::clear()
/// on this list crashes Qt 6.11.0.
void leaveAStaleCell(QComboBox* combo)
{
    // On Linux offscreen there is no accessibility bridge: QAccessible stays
    // inactive after setActive(true), the list's cells are not all built, and
    // model changes never reach QAccessibleTable. There is no stale cell to
    // plant and no crash path to reach, so only the rebuild checks run there;
    // the crash path runs where a bridge is active (macOS).
    if (!QAccessible::isActive()) {
        return;
    }
    QAccessibleInterface* list = QAccessible::queryAccessibleInterface(combo->view());
    QVERIFY(list != nullptr);
    QVERIFY(list->tableInterface() != nullptr);
    QVERIFY(list->childCount() > 0);
    for (int i = 0; i < list->childCount(); ++i) {
        QVERIFY(list->child(i) != nullptr);
    }
    QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(list->child(0)));
}

/// What the drop-down's model did.
struct ModelSpies {
    QSignalSpy removed;
    QSignalSpy inserted;
    QSignalSpy reset;
    QSignalSpy changed;

    explicit ModelSpies(QAbstractItemModel* model)
        : removed(model, &QAbstractItemModel::rowsRemoved)
        , inserted(model, &QAbstractItemModel::rowsInserted)
        , reset(model, &QAbstractItemModel::modelReset)
        , changed(model, &QAbstractItemModel::dataChanged)
    {
    }
};

struct Fixture {
    FakeRecordingsAccess access;
    DsrSetupPage page{nullptr};
    QStringList asked;
    bool answer = false;

    explicit Fixture(const QJsonObject& doc)
    {
        access.version = RecordingsAccess::kDsrVersion;
        access.entries = {saved(QStringLiteral("w1"), RecordingKind::WholePan, 900 * 1000 * 1000),
                          saved(QStringLiteral("s1"), RecordingKind::Sound, 300 * 1000 * 1000),
                          saved(QStringLiteral("c1"), RecordingKind::Contact, 200 * 1000 * 1000),
                          saved(QStringLiteral("w2"), RecordingKind::WholePan, 200 * 1000 * 1000)};
        access.setDocument(doc);
        page.setConfirm([this](const QString& title, const QString& text) {
            asked << title << text;
            return answer;
        });
        page.setAccess(&access);
    }
};

}  // namespace

class TestDsrSetupPage : public QObject {
    Q_OBJECT

private slots:
    // Spec 3: on and its status, the hours at the receivers and rates now,
    // the note, the bar with its head and red zone, and the saved stretches.
    void aFullLoopShowsEverything()
    {
        Fixture f(document(fullDsr(), ssdDisks()));
        DsrSetupPage& page = f.page;
        QCOMPARE(page.groups().size(), 3);
        QVERIFY(page.availabilityLabel()->isHidden());
        QVERIFY(page.enabledBox()->isChecked());
        QVERIFY(page.enabledBox()->isEnabled());
        QVERIFY(page.statusLabel()->text().contains(
            QStringLiteral("Recording now: 2 receivers at 192 kHz, including your own transmit audio.")));
        QVERIFY(page.sdWarning()->isHidden());

        // 10 GB at two receivers at 192 kHz: about 1 h 48 min (2.77 GB an
        // hour each, with each block's header and alignment).
        QCOMPARE(page.sizeSpin()->value(), 10);
        QCOMPARE(page.sizeSlider()->value(), 10);
        const double hours = 10.0 * kGb / double(kTwoAt192PerHour);
        QCOMPARE(page.holdsLabel()->text(),
                 QStringLiteral("Holds about <b>%1</b> with 2 receivers at 192 kHz.")
                     .arg(DsrSetupPage::durationText(hours)));
        QCOMPARE(DsrSetupPage::durationText(hours), QStringLiteral("1 h 48 min"));
        QVERIFY(page.loopNote()->text().contains(QStringLiteral("It is always erasing.")));
        QVERIFY(!page.loopNote()->isHidden());

        DsrUsageBar* bar = page.usageBar();
        QVERIFY(bar->full());
        QVERIFY(bar->moving());
        QCOMPARE(bar->headFraction(), 0.4);
        QCOMPARE(bar->eraseFraction(), 0.4);
        // The red zone is the next minute it writes over.
        QVERIFY(std::abs(bar->eraseWidthFraction() - double(kTwoAt192PerHour) / 60.0 / (10.0 * kGb)) < 1e-9);
        QVERIFY(!page.usageKey()->isHidden());
        QCOMPARE(page.usageLabel()->text(),
                 QStringLiteral("Full, erasing the oldest as it records. Kept now: 12:42 to 14:32 UTC. 412 GB "
                                "free on this disk."));

        // Saved stretches: whole pan and sound only, not contact recordings.
        QCOMPARE(page.savedLabel()->text(), QStringLiteral("3 saved, 1.4 GB."));
        QVERIFY(page.eraseButton()->isEnabled());
        QSignalSpy wanted(&page, &DsrSetupPage::recordingsWindowRequested);
        page.recordingsButton()->click();
        QCOMPARE(wanted.count(), 1);
        QVERIFY(f.access.settingsSent.isEmpty());
    }

    // The write head moves between the Core's figures: filling, the kept
    // part grows with it; full, it goes round the loop.
    void theWriteHeadMoves()
    {
        DsrUsageBar bar;
        const qint64 perHour = 3600 * kGb / 100;   // 10 MB a second
        bar.setFigures(10 * kGb, 2 * kGb, 2 * kGb, perHour, true, false);
        QVERIFY(bar.moving());
        bar.advance(1000);
        QCOMPARE(bar.headFraction(), 0.201);
        QCOMPARE(bar.keptFraction(), 0.201);
        QCOMPARE(bar.eraseWidthFraction(), 0.0);

        bar.setFigures(10 * kGb, 10 * kGb, 10 * kGb - 5 * 1000 * 1000, perHour, true, true);
        bar.advance(1000);
        QVERIFY(std::abs(bar.headFraction() - 0.0005) < 1e-9);
        QCOMPARE(bar.eraseFraction(), bar.headFraction());

        bar.setFigures(10 * kGb, 3 * kGb, 3 * kGb, perHour, false, false);
        QVERIFY(!bar.moving());
        bar.advance(1000);
        QCOMPARE(bar.headFraction(), 0.3);
    }

    // Each control sends its own field, and only on a change by hand.
    void eachControlSendsItsField()
    {
        Fixture f(document(with(fullDsr(), QStringLiteral("usedBytes"), 4 * kGb), ssdDisks()));
        DsrSetupPage& page = f.page;
        QVERIFY(page.usageLabel()->text().startsWith(QStringLiteral("Filling: 4.0 GB of 10 GB. Erasing starts "
                                                                    "when it is full, in about ")));
        QVERIFY(page.usageKey()->isHidden());

        page.enabledBox()->click();
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrEnabled"), false}}));

        page.diskCombo()->setCurrentIndex(1);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrDisk"), QStringLiteral("/Volumes/T7")}}));

        page.scanButton()->click();
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrDisk"), QString()}}));

        page.sizeSpin()->setValue(20);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrSizeLimitGb"), 20}}));
        QCOMPARE(page.sizeSlider()->value(), 20);

        // Dragging: the hours follow at once, the Core gets it on release.
        const qsizetype before = f.access.settingsSent.size();
        page.sizeSlider()->setSliderDown(true);
        page.sizeSlider()->setValue(30);
        QCOMPARE(f.access.settingsSent.size(), before);
        QCOMPARE(page.sizeSpin()->value(), 30);
        QVERIFY(page.holdsLabel()->text().contains(
            DsrSetupPage::durationText(30.0 * kGb / double(kTwoAt192PerHour))));
        page.sizeSlider()->setSliderDown(false);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrSizeLimitGb"), 30}}));

        page.formatCombo()->setCurrentIndex(1);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("format"), QStringLiteral("wav")}}));
        QVERIFY(!f.access.verbs().contains(QStringLiteral("eraseDsr")));
    }

    // A14 and spec 6.7: a size change applies on the Core, and the hours
    // follow what the Core then says.
    void aSizeChangeUpdatesTheHours()
    {
        Fixture f(document(fullDsr(), ssdDisks()));
        f.page.sizeSpin()->setValue(40);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrSizeLimitGb"), 40}}));
        // The Core applies it: the document comes back with the new limit.
        f.access.settingsSent.clear();
        QJsonObject bigger = with(with(fullDsr(), QStringLiteral("sizeLimitGb"), 40), QStringLiteral("sizeLimitBytes"),
                                  40 * kGb);
        // Not yet round the bigger loop: it writes at the end of what it keeps.
        bigger.insert(QStringLiteral("writePosition"), bigger.value(QStringLiteral("usedBytes")));
        f.access.setDocument(document(bigger, ssdDisks()));
        QCOMPARE(f.page.holdsLabel()->text(),
                 QStringLiteral("Holds about <b>7 h 13 min</b> with 2 receivers at 192 kHz."));
        QVERIFY(f.page.usageLabel()->text().startsWith(QStringLiteral("Filling: 10.0 GB of 40 GB.")));
        // Shown, not sent back.
        QVERIFY(f.access.settingsSent.isEmpty());

        // Another receiver at another rate: the hours follow it too.
        QJsonArray three = twoReceivers();
        three.append(QJsonObject{{QStringLiteral("receiver"), 2}, {QStringLiteral("rateHz"), 384000}});
        QJsonObject dsr = with(with(fullDsr(), QStringLiteral("receivers"), three), QStringLiteral("bytesPerHour"),
                               kTwoAt192PerHour + DsrRingFile::blockBytes(384000) * 3600);
        f.access.setDocument(document(dsr, ssdDisks()));
        QVERIFY(f.page.holdsLabel()->text().endsWith(QStringLiteral("with 3 receivers at 192 and 384 kHz.")));
        QVERIFY(f.access.settingsSent.isEmpty());
    }

    // The erase asks first, says it cannot be undone, and only then erases.
    void eraseAsksFirst()
    {
        Fixture f(document(fullDsr(), ssdDisks()));
        f.answer = false;
        f.page.eraseButton()->click();
        QCOMPARE(f.asked.size(), 2);
        QCOMPARE(f.asked.at(0), QStringLiteral("Erase what the recorder has kept?"));
        QCOMPARE(f.asked.at(1), QStringLiteral("This erases 10.0 GB of kept waterfall and sound. Rewinding starts "
                                               "again from now. Saved stretches stay. This cannot be undone."));
        QVERIFY(!f.access.verbs().contains(QStringLiteral("eraseDsr")));
        QVERIFY(f.page.eraseNote()->text().isEmpty());

        f.answer = true;
        f.page.eraseButton()->click();
        QCOMPARE(f.access.verbs().count(QStringLiteral("eraseDsr")), 1);
        QCOMPARE(f.page.eraseNote()->text(), QStringLiteral("Erased. The recorder keeps going from now."));
        QVERIFY(f.access.settingsSent.isEmpty());

        // Nothing kept: nothing to erase, and it says so.
        f.access.setDocument(document(with(fullDsr(), QStringLiteral("usedBytes"), 0), ssdDisks()));
        QVERIFY(!f.page.eraseButton()->isEnabled());
        QCOMPARE(f.page.eraseButton()->toolTip(), QStringLiteral("Nothing is kept now."));
        QVERIFY(!f.page.eraseButton()->isHidden());
    }

    // A14 on the page: only an SD card, so the switch is off and disabled,
    // with the strong red warning. The Core's own default (on) is not shown
    // as on.
    void anSdCardOnlyCoreDisablesTheSwitch()
    {
        QJsonObject dsr = fullDsr();
        dsr.insert(QStringLiteral("recording"), false);
        dsr.insert(QStringLiteral("usedBytes"), 0);
        dsr.insert(QStringLiteral("reason"), QStringLiteral("The only disk is an SD card."));
        Fixture f(document(dsr, sdOnlyDisks()));
        DsrSetupPage& page = f.page;
        QVERIFY(!page.enabledBox()->isChecked());
        QVERIFY(!page.enabledBox()->isEnabled());
        QVERIFY(!page.enabledBox()->isHidden());
        QCOMPARE(page.enabledBox()->toolTip(),
                 QStringLiteral("Connect a USB SSD or NVMe drive and choose it under Disk to turn the recorder on."));
        QVERIFY(!page.sdWarning()->isHidden());
        const QString warning = page.sdWarning()->text();
        QVERIFY(warning.contains(QStringLiteral("This Core needs a proper storage device for the recorder")));
        QVERIFY(warning.contains(QStringLiteral("Connect a USB SSD or an NVMe drive to the Core")));
        QVERIFY(warning.contains(QStringLiteral("A USB thumb drive is not enough")));
        QVERIFY(page.statusLabel()->text().contains(QStringLiteral("Off: this Core has no disk that can take it.")));

        // The card is listed and cannot be chosen; there is no other disk.
        auto* model = qobject_cast<QStandardItemModel*>(page.diskCombo()->model());
        QVERIFY(model != nullptr);
        QCOMPARE(page.diskCombo()->count(), 2);
        QCOMPARE(page.diskCombo()->itemText(0), QStringLiteral("SD card (58 GB free): wears out, cannot be used"));
        QVERIFY(!(model->item(0)->flags() & Qt::ItemIsEnabled));
        QCOMPARE(page.diskCombo()->itemText(1), QStringLiteral("No USB disk found"));
        QVERIFY(!(model->item(1)->flags() & Qt::ItemIsEnabled));
        QVERIFY(page.diskCombo()->isEnabled());
        QVERIFY(page.scanButton()->isEnabled());
        QVERIFY(!page.sizeSpin()->isEnabled());
        QVERIFY(!page.sizeSlider()->isEnabled());
        QVERIFY(page.holdsLabel()->isHidden());
        QVERIFY(!page.unusableNote()->isHidden());

        // Ticking it by hand does nothing: it is disabled.
        page.enabledBox()->click();
        QVERIFY(f.access.settingsSent.isEmpty());

        // A USB SSD plugged in and scanned: choosing it sends the disk.
        f.access.setDocument(document(dsr, QJsonArray{sdOnlyDisks().at(0),
                                                      disk(QStringLiteral("/media/t7"), QStringLiteral("Samsung T7"),
                                                           931 * kGb, false)}));
        QCOMPARE(page.diskCombo()->count(), 2);
        page.diskCombo()->setCurrentIndex(1);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrDisk"), QStringLiteral("/media/t7")}}));

        // Had the recorder been off, the proper disk turns it on with it.
        f.access.settingsSent.clear();
        f.access.setDocument(document(with(dsr, QStringLiteral("enabled"), false),
                                      QJsonArray{sdOnlyDisks().at(0),
                                                 disk(QStringLiteral("/media/t7"), QStringLiteral("Samsung T7"),
                                                      931 * kGb, false)}));
        page.diskCombo()->setCurrentIndex(1);
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrDisk"), QStringLiteral("/media/t7")},
                                                            {QStringLiteral("dsrEnabled"), true}}));
    }

    // Task 10b: the page shows what the Core holds. A desktop window
    // running its own radio starts with it off.
    void itShowsTheEffectiveValue()
    {
        QJsonObject dsr = fullDsr();
        dsr.insert(QStringLiteral("enabled"), false);
        dsr.insert(QStringLiteral("recording"), false);
        dsr.insert(QStringLiteral("usedBytes"), 0);
        dsr.insert(QStringLiteral("writePosition"), 0);
        Fixture f(document(dsr, ssdDisks()));
        QVERIFY(!f.page.enabledBox()->isChecked());
        QVERIFY(f.page.enabledBox()->isEnabled());
        QVERIFY(f.page.statusLabel()->text().contains(QStringLiteral("Off. Nothing is being kept.")));
        QCOMPARE(f.page.usageLabel()->text(), QStringLiteral("In use: nothing. 412 GB free on this disk."));
        QVERIFY(!f.page.usageBar()->moving());
        f.page.enabledBox()->click();
        QCOMPARE(f.access.settingsSent.last(), (QJsonObject{{QStringLiteral("dsrEnabled"), true}}));

        // On, but the radio is not streaming: no receivers, no hours.
        f.access.settingsSent.clear();
        dsr.insert(QStringLiteral("enabled"), true);
        dsr.insert(QStringLiteral("receivers"), QJsonArray());
        dsr.insert(QStringLiteral("bytesPerHour"), 0);
        f.access.setDocument(document(dsr, ssdDisks()));
        QVERIFY(f.page.enabledBox()->isChecked());
        QVERIFY(f.page.statusLabel()->text().contains(QStringLiteral("On. It records while the radio is connected.")));
        QCOMPARE(f.page.holdsLabel()->text(), QStringLiteral("How long it holds is shown while the radio is connected."));
        QVERIFY(f.access.settingsSent.isEmpty());
    }

    // V9: an older Core leaves every control in place, disabled, with the
    // reason; so does no connection.
    void anOlderCoreOrNoConnectionDimsTheGroups()
    {
        Fixture f(document(fullDsr(), ssdDisks()));
        f.access.version = 3;
        f.access.setDocument(f.access.document());
        DsrSetupPage& page = f.page;
        const QString reason = RecordingsAccess::needsNewerCoreReason();
        QCOMPARE(page.availabilityLabel()->text(), reason);
        QVERIFY(!page.availabilityLabel()->isHidden());
        for (QGroupBox* group : page.groups()) {
            QVERIFY(!group->isEnabled());
            QVERIFY(!group->isHidden());
            QCOMPARE(group->toolTip(), reason);
        }
        for (QWidget* w : std::initializer_list<QWidget*>{page.enabledBox(), page.diskCombo(), page.scanButton(),
                                                          page.sizeSlider(), page.sizeSpin(),
                                                          page.recordingsButton(), page.formatCombo(),
                                                          page.eraseButton()}) {
            QVERIFY(!w->isEnabled() || !w->parentWidget()->isEnabled());
            QVERIFY(!w->isHidden());
        }
        QVERIFY(!page.eraseButton()->isEnabled());
        QCOMPARE(page.eraseButton()->toolTip(), reason);
        page.eraseButton()->click();
        QVERIFY(f.asked.isEmpty());

        f.access.version = RecordingsAccess::kDsrVersion;
        f.access.setState(RecordingsAccess::State::NotConnected);
        QCOMPARE(page.availabilityLabel()->text(), RecordingsAccess::notConnectedReason());
        QVERIFY(page.statusLabel()->text().contains(QStringLiteral("Not known: no Core connected.")));
        QCOMPARE(page.savedLabel()->text(), QStringLiteral("Not known."));
        QVERIFY(f.access.settingsSent.isEmpty());
        QVERIFY(!f.access.verbs().contains(QStringLiteral("eraseDsr")));
    }

    // PR 808 crash: the Core sends the recorder's figures about once a
    // second. The same disks change only the words and figures in place;
    // other disks reset the list's model, never removing rows one by one,
    // which crashed Qt's accessibility on JJ's Mac.
    void coreUpdatesDoNotRebuildTheDisks()
    {
        AccessibilityOn accessibility;
        Fixture f(document(fullDsr(), ssdDisks()));
        QComboBox* combo = f.page.diskCombo();
        QCOMPARE(combo->count(), 2);
        leaveAStaleCell(combo);
        ModelSpies spies(combo->model());

        for (int n = 1; n <= 5; ++n) {
            f.access.setDocument(secondLater(n));
            QCOMPARE(combo->count(), 2);
            QCOMPARE(combo->currentIndex(), 0);
            QCOMPARE(combo->itemText(0), QStringLiteral("The Core's own disk (SSD, %1 GB free)").arg(412 - n));
            QCOMPARE(combo->itemData(0, Qt::UserRole + 2).toLongLong(), (412 - n) * kGb);
        }
        // Words and the free figure changed in place; nothing else.
        QCOMPARE(spies.removed.count(), 0);
        QCOMPARE(spies.inserted.count(), 0);
        QCOMPARE(spies.reset.count(), 0);
        QVERIFY(spies.changed.count() > 0);

        // The same document again: not touched at all.
        spies.changed.clear();
        f.access.setDocument(secondLater(5));
        QCOMPARE(spies.changed.count(), 0);

        // Another disk plugged in: one reset, the stale cell survives it.
        QJsonArray three = ssdDisks();
        three.append(disk(QStringLiteral("/Volumes/NVMe"), QStringLiteral("USB disk: NVMe"), 1800 * kGb, false));
        f.access.setDocument(document(fullDsr(), three));
        QCOMPARE(spies.reset.count(), 1);
        QCOMPARE(spies.removed.count(), 0);
        QCOMPARE(combo->count(), 3);
        QCOMPARE(combo->currentIndex(), 0);
        QCOMPARE(combo->itemData(2).toString(), QStringLiteral("/Volumes/NVMe"));
        QCOMPARE(combo->itemData(2, Qt::UserRole + 1).toBool(), false);

        // Unplugged again, with another stale cell: the same.
        leaveAStaleCell(combo);
        f.access.setDocument(document(fullDsr(), ssdDisks()));
        QCOMPARE(spies.reset.count(), 2);
        QCOMPARE(spies.removed.count(), 0);
        QCOMPARE(combo->count(), 2);

        // An SD card only Core: still the disabled lines (A14).
        leaveAStaleCell(combo);
        f.access.setDocument(document(fullDsr(), sdOnlyDisks()));
        QCOMPARE(combo->count(), 2);
        auto* model = qobject_cast<QStandardItemModel*>(combo->model());
        QVERIFY(model != nullptr);
        QVERIFY(!model->item(0)->isEnabled());
        QCOMPARE(combo->itemText(1), QStringLiteral("No USB disk found"));
        QVERIFY(!model->item(1)->isEnabled());
        QVERIFY(f.access.settingsSent.isEmpty());
    }

    // Other disks while the list is open: it is left alone until it closes,
    // then filled from the latest document. Figures still change in place.
    void aDiskChangeWaitsForTheListToClose()
    {
        AccessibilityOn accessibility;
        Fixture f(document(fullDsr(), ssdDisks()));
        f.page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.page));
        QComboBox* combo = f.page.diskCombo();
        combo->showPopup();
        QTRY_VERIFY(combo->view()->isVisible());
        leaveAStaleCell(combo);
        ModelSpies spies(combo->model());

        f.access.setDocument(secondLater(1));
        QCOMPARE(combo->itemText(0), QStringLiteral("The Core's own disk (SSD, 411 GB free)"));

        QJsonArray three = ssdDisks();
        three.append(disk(QStringLiteral("/Volumes/NVMe"), QStringLiteral("USB disk: NVMe"), 1800 * kGb, false));
        f.access.setDocument(document(fullDsr(), three));
        f.access.setDocument(document(fullDsr(), three));
        QCOMPARE(combo->count(), 2);
        QCOMPARE(spies.reset.count(), 0);
        QCOMPARE(spies.removed.count(), 0);
        QCOMPARE(spies.inserted.count(), 0);

        combo->hidePopup();
        QTRY_COMPARE(combo->count(), 3);
        QCOMPARE(spies.reset.count(), 1);
        QCOMPARE(spies.removed.count(), 0);
        QCOMPARE(combo->currentIndex(), 0);
        QVERIFY(f.access.settingsSent.isEmpty());
    }

    void wordsForTimesAndReceivers()
    {
        QCOMPARE(DsrSetupPage::durationText(0.5), QStringLiteral("30 min"));
        QCOMPARE(DsrSetupPage::durationText(2.0), QStringLiteral("2 h"));
        QCOMPARE(DsrSetupPage::durationText(181.5), QStringLiteral("7.6 days"));
        QCOMPARE(DsrSetupPage::receiversText(QJsonArray{QJsonObject{{QStringLiteral("rateHz"), 1536000}}}),
                 QStringLiteral("1 receiver at 1536 kHz"));
        QCOMPARE(DsrSetupPage::receiversText(QJsonArray()), QString());
    }
};

QTEST_MAIN(TestDsrSetupPage)
#include "tst_dsr_setup_page.moc"
