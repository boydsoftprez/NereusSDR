// no-port-check: NereusSDR-original persistence adapter regression tests.
// 2026-09-25: iPhone app plan Task 73 (R-IOS-02, ruling 5.3): owners and
// held-for marks in the manifest. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/ReceiveLayoutStore.h"
#include "core/SliceOwnership.h"
#include "core/WdspEngine.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <limits>

using namespace NereusSDR;

namespace {

constexpr auto kLayoutKey = "receiveLayout";
const QString kMacA = QStringLiteral("AA:BB:CC:DD:EE:01");
const QString kMacB = QStringLiteral("AA:BB:CC:DD:EE:02");

QList<ReceiveSliceState> twoSlices()
{
    return {
        {0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB},
        {2, QStringLiteral("pan-1"), 7200000.0, DSPMode::LSB},
    };
}

ReceiveLayoutStore::LoadResult persistRawAndLoad(const QString& path,
                                                 const QString& mac,
                                                 const QString& raw)
{
    {
        AppSettings settings(path);
        settings.setHardwareValue(AppSettings::normalizedRadioMac(mac),
                                  QLatin1String(kLayoutKey), raw);
        if (!settings.save()) {
            qFatal("Could not persist test settings file");
        }
    }
    AppSettings reloaded(path);
    reloaded.load();
    return ReceiveLayoutStore::load(reloaded, mac);
}

} // namespace

class TstReceiveLayoutStore : public QObject {
    Q_OBJECT

private slots:
    void stageRoundTripsNoncontiguousIdsAndOrder()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings settings(path);
            QString error;
            auto saved = twoSlices();
            saved.swapItemsAt(0, 1);
            QVERIFY2(ReceiveLayoutStore::stage(settings, kMacA, saved, &error),
                     qPrintable(error));
            QVERIFY2(settings.save(), "Could not persist test settings file");
        }

        AppSettings reloaded(path);
        reloaded.load();
        const auto result = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(result.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(result.slices.size(), 2);
        QCOMPARE(result.slices.at(0).id, 2);
        QCOMPARE(result.slices.at(1).id, 0);
        QCOMPARE(result.slices.at(0).panKey, QStringLiteral("pan-1"));
        QCOMPARE(result.slices.at(1).panKey, QStringLiteral("pan-0"));
        QCOMPARE(result.slices.at(0).frequencyHz, 7200000.0);
        QCOMPARE(result.slices.at(1).frequencyHz, 14293200.0);
        QCOMPARE(static_cast<int>(result.slices.at(0).dspMode), static_cast<int>(DSPMode::LSB));
        QCOMPARE(static_cast<int>(result.slices.at(1).dspMode), static_cast<int>(DSPMode::USB));
    }

    void macNamespacesNormalizeSeparatorsAndRemainSeparate()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QString dashedMac = QStringLiteral("aa-bb-cc-dd-ee-01");

        {
            AppSettings settings(path);
            auto layoutA = twoSlices();
            auto layoutB = QList<ReceiveSliceState>{
                {1, QStringLiteral("pan-3"), 10100000.0, DSPMode::AM},
            };
            QVERIFY(ReceiveLayoutStore::stage(settings, dashedMac, layoutA));
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacB, layoutB));
            QVERIFY(settings.save());
        }

        AppSettings reloaded(path);
        reloaded.load();
        const auto a = ReceiveLayoutStore::load(reloaded, kMacA);
        const auto b = ReceiveLayoutStore::load(reloaded, kMacB);
        QCOMPARE(static_cast<int>(a.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(static_cast<int>(b.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(a.slices.at(1).id, 2);
        QCOMPARE(b.slices.size(), 1);
        QCOMPARE(b.slices.at(0).id, 1);
        QCOMPARE(b.slices.at(0).panKey, QStringLiteral("pan-3"));
    }

    void stagingReplacementPersistsRemovedMembership()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings settings(path);
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, twoSlices()));
            QVERIFY(settings.save());
        }
        {
            AppSettings settings(path);
            settings.load();
            const QList<ReceiveSliceState> justA{
                {0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB},
            };
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, justA));
            QVERIFY(settings.save());
        }

        AppSettings reloaded(path);
        reloaded.load();
        const auto result = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(result.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(result.slices.size(), 1);
        QCOMPARE(result.slices.first().id, 0);
    }

    void sharedPanAndEmptyCapturePanNormalizeToDefault()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QList<ReceiveSliceState> captured{
            {0, {}, 14293200.0, DSPMode::USB},
            {1, QStringLiteral("pan-0"), 14294000.0, DSPMode::USB},
        };

        {
            AppSettings settings(path);
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, captured));
            QVERIFY(settings.save());
        }

        AppSettings reloaded(path);
        reloaded.load();
        const auto result = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(result.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(result.slices.at(0).panKey, QStringLiteral("pan-0"));
        QCOMPARE(result.slices.at(1).panKey, QStringLiteral("pan-0"));
    }

    void singleRadeSliceInfersItsReceiveAudioOwnerAcrossDisk()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QList<ReceiveSliceState> slices{
            {0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB},
            {2, QStringLiteral("pan-1"), 7200000.0, DSPMode::RADE_L},
        };

        {
            AppSettings settings(path);
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, slices));
            QVERIFY(settings.save());
        }
        AppSettings reloaded(path);
        reloaded.load();
        const auto result = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(result.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QVERIFY(result.radeRxOwnerId.has_value());
        QCOMPARE(*result.radeRxOwnerId, 2);
    }

    void multipleRadeSlicesPersistExplicitSecondOwnerAndOrder()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QList<ReceiveSliceState> slices{
            {4, QStringLiteral("pan-4"), 14293200.0, DSPMode::RADE_L},
            {1, QStringLiteral("pan-1"), 7200000.0, DSPMode::RADE_U},
        };

        {
            AppSettings settings(path);
            QString error;
            QVERIFY2(ReceiveLayoutStore::stage(settings, kMacA, slices, &error, 1),
                     qPrintable(error));
            QVERIFY(settings.save());
        }
        AppSettings reloaded(path);
        reloaded.load();
        const auto result = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(result.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(result.slices.at(0).id, 4);
        QCOMPARE(result.slices.at(1).id, 1);
        QVERIFY(result.radeRxOwnerId.has_value());
        QCOMPARE(*result.radeRxOwnerId, 1);
    }

    void noRadeOwnerRoundTripsAsNullAndOldFormatStillLoads()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        {
            AppSettings settings(path);
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, twoSlices()));
            const QString raw = settings.hardwareValue(kMacA, QLatin1String(kLayoutKey)).toString();
            const QJsonDocument document = QJsonDocument::fromJson(raw.toUtf8());
            QVERIFY(document.isObject());
            QVERIFY(document.object().value(QStringLiteral("radeRxOwnerId")).isNull());
            QVERIFY(settings.save());
        }
        AppSettings reloaded(path);
        reloaded.load();
        const auto current = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(current.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QVERIFY(!current.radeRxOwnerId.has_value());

        const QString oldFormat = QStringLiteral(
            R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":14293200,"dspMode":1},{"id":2,"panKey":"pan-1","frequencyHz":7200000,"dspMode":0}]})");
        const auto old = persistRawAndLoad(path, kMacA, oldFormat);
        QCOMPARE(static_cast<int>(old.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QVERIFY(!old.radeRxOwnerId.has_value());
    }

    void missingAndInvalidIdentityAreDistinct()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("NereusSDR.settings")));

        QCOMPARE(static_cast<int>(ReceiveLayoutStore::load(settings, kMacA).state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Missing));
        QCOMPARE(static_cast<int>(ReceiveLayoutStore::load(settings, QStringLiteral("not-a-mac")).state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::InvalidIdentity));
        QString error;
        QVERIFY(!ReceiveLayoutStore::stage(settings, QStringLiteral("not-a-mac"),
                                            twoSlices(), &error));
        QVERIFY(!error.isEmpty());
    }

    void stageOnlyUpdatesMemoryUntilAppSettingsSaves()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));

        AppSettings settings(path);
        QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, twoSlices()));
        QVERIFY(!QFile::exists(path));

        AppSettings beforeSave(path);
        beforeSave.load();
        QCOMPARE(static_cast<int>(ReceiveLayoutStore::load(beforeSave, kMacA).state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Missing));

        QVERIFY(settings.save());
        AppSettings afterSave(path);
        afterSave.load();
        QCOMPARE(static_cast<int>(ReceiveLayoutStore::load(afterSave, kMacA).state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
    }

    void corruptStoredLayoutsAreRejectedAsWhole_data()
    {
        QTest::addColumn<QString>("raw");
        QTest::newRow("malformed") << QStringLiteral("{");
        QTest::newRow("wrong-version-type")
            << QStringLiteral(R"({"version":"1","slices":[]})");
        QTest::newRow("fractional-version")
            << QStringLiteral(R"({"version":1.5,"slices":[]})");
        QTest::newRow("future-version")
            << QStringLiteral(R"({"version":2,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("missing-slices") << QStringLiteral(R"({"version":1})");
        QTest::newRow("empty-membership") << QStringLiteral(R"({"version":1,"slices":[]})");
        QTest::newRow("fractional-id")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0.5,"panKey":"pan-0","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("string-id")
            << QStringLiteral(R"({"version":1,"slices":[{"id":"0","panKey":"pan-0","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("out-of-range-id")
            << QStringLiteral(R"({"version":1,"slices":[{"id":5,"panKey":"pan-0","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("string-frequency")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":"1","dspMode":1}]})");
        QTest::newRow("fractional-mode")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1.5}]})");
        QTest::newRow("invalid-mode")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":99}]})");
        QTest::newRow("noncanonical-pan")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-00","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("empty-stored-pan")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("missing-pan")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("duplicate-ids")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1},{"id":0,"panKey":"pan-1","frequencyHz":2,"dspMode":0}]})");
        QTest::newRow("out-of-range-frequency")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":61440000.1,"dspMode":1}]})");
        QTest::newRow("negative-frequency")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":-1,"dspMode":1}]})");
        QTest::newRow("too-many-members")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1},{"id":1,"panKey":"pan-1","frequencyHz":1,"dspMode":1},{"id":2,"panKey":"pan-2","frequencyHz":1,"dspMode":1},{"id":3,"panKey":"pan-3","frequencyHz":1,"dspMode":1},{"id":4,"panKey":"pan-4","frequencyHz":1,"dspMode":1},{"id":0,"panKey":"pan-5","frequencyHz":1,"dspMode":1}]})");
        QTest::newRow("unknown-field")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1}],"extra":true})");
        QTest::newRow("ambiguous-old-rade-owner")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":12},{"id":2,"panKey":"pan-1","frequencyHz":2,"dspMode":13}]})");
        QTest::newRow("ambiguous-null-rade-owner")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":12},{"id":2,"panKey":"pan-1","frequencyHz":2,"dspMode":13}],"radeRxOwnerId":null})");
        QTest::newRow("owner-is-usb-slice")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1},{"id":2,"panKey":"pan-1","frequencyHz":2,"dspMode":13}],"radeRxOwnerId":0})");
        QTest::newRow("stale-owner")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":13}],"radeRxOwnerId":2})");
        QTest::newRow("out-of-range-owner")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":13}],"radeRxOwnerId":5})");
        QTest::newRow("fractional-owner")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":13}],"radeRxOwnerId":0.5})");
        QTest::newRow("wrong-type-owner")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":13}],"radeRxOwnerId":"0"})");
        QTest::newRow("owner-without-rade")
            << QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":1,"dspMode":1}],"radeRxOwnerId":0})");
        QTest::newRow("oversize") << QString(17000, QLatin1Char('x'));
    }

    void corruptStoredLayoutsAreRejectedAsWhole()
    {
        QFETCH(QString, raw);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const auto result = persistRawAndLoad(path, kMacA, raw);
        QCOMPARE(static_cast<int>(result.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::InvalidData));
        QVERIFY(result.slices.isEmpty());
        QVERIFY(!result.error.isEmpty());
        AppSettings retained(path);
        retained.load();
        QCOMPARE(retained.hardwareValue(kMacA, QLatin1String(kLayoutKey)).toString(), raw);
    }

    void invalidStagePreservesExistingValueIncludingNonfiniteInput()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, twoSlices()));
        const QString original = settings.hardwareValue(
            AppSettings::normalizedRadioMac(kMacA), QLatin1String(kLayoutKey)).toString();

        auto invalid = twoSlices();
        invalid[0].frequencyHz = std::numeric_limits<double>::quiet_NaN();
        QString error;
        QVERIFY(!ReceiveLayoutStore::stage(settings, kMacA, invalid, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(settings.hardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                        QLatin1String(kLayoutKey)).toString(), original);

        invalid = twoSlices();
        invalid[0].frequencyHz = std::numeric_limits<double>::infinity();
        QVERIFY(!ReceiveLayoutStore::stage(settings, kMacA, invalid, &error));
        QCOMPARE(settings.hardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                        QLatin1String(kLayoutKey)).toString(), original);

        invalid = twoSlices();
        invalid[0].id = WdspEngine::kMaxSliceChannels;
        QVERIFY(!ReceiveLayoutStore::stage(settings, kMacA, invalid, &error));
        QCOMPARE(settings.hardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                        QLatin1String(kLayoutKey)).toString(), original);

        const QList<ReceiveSliceState> twoRade{
            {0, QStringLiteral("pan-0"), 14293200.0, DSPMode::RADE_U},
            {2, QStringLiteral("pan-1"), 7200000.0, DSPMode::RADE_L},
        };
        QVERIFY(!ReceiveLayoutStore::stage(settings, kMacA, twoRade, &error));
        QCOMPARE(settings.hardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                        QLatin1String(kLayoutKey)).toString(), original);
        QVERIFY(!ReceiveLayoutStore::stage(settings, kMacA, twoRade, &error, 4));
        QCOMPARE(settings.hardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                        QLatin1String(kLayoutKey)).toString(), original);
    }

    // iPhone app Task 73 (ruling 5.3): each entry keeps its owner, or the
    // device it is held for, across a restart.
    void ownersAndHeldForMarksRoundTripThroughTheFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QByteArray phone(32, '\x11');
        const QByteArray tablet(32, '\x22');
        QList<ReceiveSliceState> slices = twoSlices();
        slices[0].owner = phone;
        slices[1].owner = SliceOwnership::stationDevice();
        slices[1].heldFor = tablet;
        slices.append({3, QStringLiteral("pan-0"), 3573000.0, DSPMode::LSB});
        {
            AppSettings settings(path);
            QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, slices));
            QVERIFY(settings.save());
        }
        AppSettings reloaded(path);
        reloaded.load();
        const auto loaded = ReceiveLayoutStore::load(reloaded, kMacA);
        QCOMPARE(static_cast<int>(loaded.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(loaded.slices.size(), 3);
        QCOMPARE(loaded.slices.at(0).owner, phone);
        QVERIFY(loaded.slices.at(0).heldFor.isEmpty());
        QCOMPARE(loaded.slices.at(1).owner, SliceOwnership::stationDevice());
        QCOMPARE(loaded.slices.at(1).heldFor, tablet);
        // No owner is written as none.
        QVERIFY(loaded.slices.at(2).owner.isEmpty());
        QVERIFY(loaded.slices.at(2).heldFor.isEmpty());
    }

    // A manifest from before owners restores every slice with no owner, and
    // an unowned manifest is written exactly as before.
    void aManifestFromBeforeOwnersRestoresUnownedSlices()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QString oldFormat = QStringLiteral(
            R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":14293200,"dspMode":1}],"radeRxOwnerId":null})");
        const auto old = persistRawAndLoad(path, kMacA, oldFormat);
        QCOMPARE(static_cast<int>(old.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QVERIFY(old.slices.at(0).owner.isEmpty());
        QVERIFY(old.slices.at(0).heldFor.isEmpty());

        AppSettings settings(directory.filePath(QStringLiteral("Other.settings")));
        QVERIFY(ReceiveLayoutStore::stage(settings, kMacA,
                                          {{0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB}}));
        const QString raw = settings.hardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                                   QLatin1String(kLayoutKey)).toString();
        QVERIFY(!raw.contains(QStringLiteral("owner")));
        QVERIFY(!raw.contains(QStringLiteral("heldFor")));
    }

    // A window signed in with the older token cannot be recognised when it
    // comes back, so its slices are written with no owner.
    void aTokenWindowsSlicesAreWrittenWithNoOwner()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("NereusSDR.settings")));
        QList<ReceiveSliceState> slices = twoSlices();
        slices[0].owner = QByteArrayLiteral("token:3");
        QVERIFY(ReceiveLayoutStore::stage(settings, kMacA, slices));
        const auto loaded = ReceiveLayoutStore::load(settings, kMacA);
        QVERIFY(loaded.slices.at(0).owner.isEmpty());
    }

    void aHeldMarkWithoutTheStationDeviceIsRefused()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("NereusSDR.settings"));
        const QString heldByAPhone = QStringLiteral(
            R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":14293200,"dspMode":1,"owner":"ERERERERERERERERERERERERERERERERERERERERERE","heldFor":"IiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiI"}]})");
        const auto loaded = persistRawAndLoad(path, kMacA, heldByAPhone);
        QCOMPARE(static_cast<int>(loaded.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::InvalidData));
    }
};

QTEST_MAIN(TstReceiveLayoutStore)
#include "tst_receive_layout_store.moc"
