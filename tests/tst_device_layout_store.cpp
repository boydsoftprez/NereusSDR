// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_device_layout_store.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 73 (R-IOS-02; the several-devices design, section
// 5.3, ruling 5.3): DeviceLayoutStore, each device's closed slices kept for
// its return, per device and per radio.
//
// A record round-trips through the settings file; at most maxSlices entries
// per device, the oldest going first, one per id; devices and radios kept
// apart; a revoked device forgotten on every radio; an unreadable record read
// as none. The settings copy: every key of the slice's own ("Slice<id>/...",
// every band, and its per-radio "hardware/<mac>/slices/<id>/..."), none of
// another slice's; written back under another letter after that letter's
// own keys are cleared; cleared from its old letter.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/DeviceLayoutStore.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:01");
const QString kOtherMac = QStringLiteral("AA:BB:CC:DD:EE:02");
const QByteArray kPhone = QByteArray(32, '\x11');
const QByteArray kTablet = QByteArray(32, '\x22');

SavedSlice saved(int id, double hz, DSPMode mode = DSPMode::USB)
{
    SavedSlice slice;
    slice.id = id;
    slice.panKey = QStringLiteral("pan-0");
    slice.frequencyHz = hz;
    slice.dspMode = mode;
    slice.settings.insert(QStringLiteral("Slice/AgcMode"), QString::number(id));
    return slice;
}

QList<int> idsOf(const QList<SavedSlice>& slices)
{
    QList<int> ids;
    for (const SavedSlice& slice : slices) {
        ids.append(slice.id);
    }
    return ids;
}

} // namespace

class TstDeviceLayoutStore : public QObject {
    Q_OBJECT

private slots:
    void aRecordRoundTripsThroughTheFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("NereusSDR.settings"));
        {
            AppSettings settings(path);
            SavedSlice first = saved(1, 7074000.0, DSPMode::DIGU);
            first.panKey = QStringLiteral("pan-1");
            first.settings.insert(QStringLiteral("Slice/Band40m/Frequency"), QStringLiteral("7074000"));
            first.settings.insert(QStringLiteral("radio/nnr/Model"), QStringLiteral("dfnr"));
            QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone, first, 5));
            QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone, saved(3, 14074000.0), 5));
            QVERIFY(settings.save());
        }
        AppSettings reloaded(path);
        reloaded.load();
        const QList<SavedSlice> slices = DeviceLayoutStore::load(reloaded, kMac, kPhone);
        QCOMPARE(idsOf(slices), (QList<int>{1, 3}));
        QCOMPARE(slices.at(0).panKey, QStringLiteral("pan-1"));
        QCOMPARE(slices.at(0).frequencyHz, 7074000.0);
        QCOMPARE(slices.at(0).dspMode, DSPMode::DIGU);
        QCOMPARE(slices.at(0).settings.value(QStringLiteral("Slice/Band40m/Frequency")),
                 QStringLiteral("7074000"));
        QCOMPARE(slices.at(0).settings.value(QStringLiteral("radio/nnr/Model")),
                 QStringLiteral("dfnr"));
    }

    void atMostMaxSlicesPerDeviceTheOldestGoingFirst()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        for (int id = 0; id < 4; ++id) {
            QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone,
                                              saved(id, 7000000.0 + id * 1000.0), 3));
        }
        QCOMPARE(idsOf(DeviceLayoutStore::load(settings, kMac, kPhone)), (QList<int>{1, 2, 3}));
        // One entry per id: the same letter saved again replaces it, as the
        // newest.
        QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone, saved(1, 3573000.0), 3));
        const QList<SavedSlice> slices = DeviceLayoutStore::load(settings, kMac, kPhone);
        QCOMPARE(idsOf(slices), (QList<int>{2, 3, 1}));
        QCOMPARE(slices.last().frequencyHz, 3573000.0);
    }

    void devicesAndRadiosAreKeptApart()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone, saved(0, 7000000.0), 5));
        QVERIFY(DeviceLayoutStore::append(settings, kMac, kTablet, saved(1, 14000000.0), 5));
        QVERIFY(DeviceLayoutStore::append(settings, kOtherMac, kPhone, saved(2, 21000000.0), 5));
        QCOMPARE(idsOf(DeviceLayoutStore::load(settings, kMac, kPhone)), (QList<int>{0}));
        QCOMPARE(idsOf(DeviceLayoutStore::load(settings, kMac, kTablet)), (QList<int>{1}));
        QCOMPARE(idsOf(DeviceLayoutStore::load(settings, kOtherMac, kPhone)), (QList<int>{2}));
    }

    void aRevokedDeviceIsForgottenOnEveryRadio()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone, saved(0, 7000000.0), 5));
        QVERIFY(DeviceLayoutStore::append(settings, kOtherMac, kPhone, saved(2, 21000000.0), 5));
        QVERIFY(DeviceLayoutStore::append(settings, kMac, kTablet, saved(1, 14000000.0), 5));
        DeviceLayoutStore::forgetDevice(settings, kPhone);
        QVERIFY(DeviceLayoutStore::load(settings, kMac, kPhone).isEmpty());
        QVERIFY(DeviceLayoutStore::load(settings, kOtherMac, kPhone).isEmpty());
        QCOMPARE(idsOf(DeviceLayoutStore::load(settings, kMac, kTablet)), (QList<int>{1}));
    }

    void replacingWithNothingRemovesTheRecord()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(DeviceLayoutStore::append(settings, kMac, kPhone, saved(0, 7000000.0), 5));
        QVERIFY(DeviceLayoutStore::replace(settings, kMac, kPhone, {}, 5));
        QVERIFY(DeviceLayoutStore::load(settings, kMac, kPhone).isEmpty());
        QVERIFY(!settings.contains(QStringLiteral("hardware/%1/%2")
                                       .arg(AppSettings::normalizedRadioMac(kMac),
                                            DeviceLayoutStore::recordKey(kPhone))));
    }

    void anUnreadableRecordReadsAsNoneAndABadSliceIsRefused()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        settings.setHardwareValue(AppSettings::normalizedRadioMac(kMac),
                                  DeviceLayoutStore::recordKey(kPhone), QStringLiteral("{nope"));
        QVERIFY(DeviceLayoutStore::load(settings, kMac, kPhone).isEmpty());
        QVERIFY(!DeviceLayoutStore::append(settings, kMac, kPhone, saved(9, 7000000.0), 5));
        QVERIFY(!DeviceLayoutStore::append(settings, kMac, kPhone, saved(0, -1.0), 5));
        QVERIFY(!DeviceLayoutStore::append(settings, QString(), kPhone, saved(0, 7000000.0), 5));
        QVERIFY(!DeviceLayoutStore::append(settings, kMac, QByteArray(), saved(0, 7000000.0), 5));
    }

    void aSlicesOwnSettingsAreCopiedClearedAndWrittenUnderAnotherLetter()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = AppSettings::normalizedRadioMac(kMac);
        settings.setValue(QStringLiteral("Slice1/Band20m/Frequency"), QStringLiteral("14074000"));
        settings.setValue(QStringLiteral("Slice1/Band40m/ModeUSB/FilterLow"), QStringLiteral("100"));
        settings.setValue(QStringLiteral("Slice1/AfGain"), QStringLiteral("42"));
        settings.setValue(QStringLiteral("hardware/%1/slices/1/nnr/Model").arg(mac),
                          QStringLiteral("dfnr"));
        // Another slice's, and a longer id sharing the first digit.
        settings.setValue(QStringLiteral("Slice2/AfGain"), QStringLiteral("7"));
        settings.setValue(QStringLiteral("Slice10/AfGain"), QStringLiteral("9"));
        settings.setValue(QStringLiteral("hardware/%1/slices/2/nnr/Model").arg(mac),
                          QStringLiteral("rnnoise"));

        const QMap<QString, QString> copy =
            DeviceLayoutStore::captureSliceSettings(settings, kMac, 1);
        QCOMPARE(copy.size(), 4);
        QCOMPARE(copy.value(QStringLiteral("Slice/Band20m/Frequency")), QStringLiteral("14074000"));
        QCOMPARE(copy.value(QStringLiteral("Slice/Band40m/ModeUSB/FilterLow")), QStringLiteral("100"));
        QCOMPARE(copy.value(QStringLiteral("Slice/AfGain")), QStringLiteral("42"));
        QCOMPARE(copy.value(QStringLiteral("radio/nnr/Model")), QStringLiteral("dfnr"));

        DeviceLayoutStore::clearSliceSettings(settings, kMac, 1);
        QVERIFY(!settings.contains(QStringLiteral("Slice1/AfGain")));
        QVERIFY(!settings.contains(QStringLiteral("hardware/%1/slices/1/nnr/Model").arg(mac)));
        QCOMPARE(settings.value(QStringLiteral("Slice10/AfGain")).toString(), QStringLiteral("9"));

        // Letter C was in use by someone else before: what it had is gone.
        DeviceLayoutStore::writeSliceSettings(settings, kMac, 2, copy);
        QCOMPARE(settings.value(QStringLiteral("Slice2/AfGain")).toString(), QStringLiteral("42"));
        QCOMPARE(settings.value(QStringLiteral("Slice2/Band20m/Frequency")).toString(),
                 QStringLiteral("14074000"));
        QCOMPARE(settings.value(QStringLiteral("hardware/%1/slices/2/nnr/Model").arg(mac))
                     .toString(),
                 QStringLiteral("dfnr"));
        QCOMPARE(DeviceLayoutStore::captureSliceSettings(settings, kMac, 2), copy);
    }
};

QTEST_GUILESS_MAIN(TstDeviceLayoutStore)
#include "tst_device_layout_store.moc"
