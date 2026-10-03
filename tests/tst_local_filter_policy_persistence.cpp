// no-port-check: NereusSDR-original test. A local window's filter policy
// is saved for its radio as soon as it changes.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_local_filter_policy_persistence.cpp  (NereusSDR)
// =================================================================
//
// R-R3-46 / R-R3-21. The filter policy dialog's Apply in a local window
// changes AlexController's policy, which AlexController::save() writes as
// hardware/<mac>/alex/antenna/Alex{0,1}_BpfMode. Nothing marked the
// controller for saving on a policy change, so the choice reached disk only
// when some other antenna change happened to trigger a save. This test
// uses its own settings profile (a private file), applies a policy through
// the local dialog, and reads the file back from disk.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/accessories/AlexController.h"
#include "gui/widgets/FilterPolicyDialog.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:42");
}

class TstLocalFilterPolicyPersistence : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // A private settings file: this process's own profile, set before
        // the settings singleton is first used.
        QStandardPaths::setTestModeEnabled(true);
        AppSettings::setProfileOverride(QStringLiteral("local-filter-policy-%1")
                                            .arg(QCoreApplication::applicationPid()));
        QFile::remove(AppSettings::instance().filePath());
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        QDir(QFileInfo(AppSettings::instance().filePath()).absolutePath()).removeRecursively();
    }

    void localPolicyChangeIsSavedForTheRadio()
    {
        RadioModel model;
        QVERIFY(model.ownsLocalDsp());
        model.alexControllerMutable().setMacAddress(kMac);
        const QString path = AppSettings::instance().filePath();
        const QString key0 = QStringLiteral("hardware/%1/alex/antenna/Alex0_BpfMode").arg(kMac);
        const QString key1 = QStringLiteral("hardware/%1/alex/antenna/Alex1_BpfMode").arg(kMac);

        {
            FilterPolicyDialog dialog(0, &model);
            auto* bypass = dialog.findChild<QRadioButton*>(QStringLiteral("filterPolicyForceBypass"));
            auto* apply = dialog.findChild<QPushButton*>(QStringLiteral("filterPolicyApply"));
            QVERIFY(bypass && apply);
            bypass->setChecked(true);
            apply->click();
        }
        QCOMPARE(model.alexController().bpfMode(0), AlexController::BpfMode::ForceBypass);

        // Read back from disk, not from memory.
        const auto onDisk = [&path](const QString& key) {
            AppSettings disk(path);
            disk.load();
            return disk.value(key).toString();
        };
        QTRY_COMPARE(onDisk(key0), QStringLiteral("2"));

        // A chain locked to wideband keeps its bypass, but a new policy for
        // it is still saved.
        model.alexControllerMutable().setWidebandActive(1, true);
        {
            FilterPolicyDialog dialog(1, &model);
            auto* force = dialog.findChild<QRadioButton*>(QStringLiteral("filterPolicyForceFilter"));
            auto* apply = dialog.findChild<QPushButton*>(QStringLiteral("filterPolicyApply"));
            QVERIFY(force && apply);
            force->setChecked(true);
            apply->click();
        }
        QCOMPARE(model.alexController().adcState(1).effective,
                 AlexController::BpfEffective::WidebandLocked);
        QTRY_COMPARE(onDisk(key1), QStringLiteral("1"));

        // A fresh controller for the same radio loads both choices.
        AppSettings::instance().load();
        AlexController reloaded;
        reloaded.setMacAddress(kMac);
        reloaded.load();
        QCOMPARE(reloaded.bpfMode(0), AlexController::BpfMode::ForceBypass);
        QCOMPARE(reloaded.bpfMode(1), AlexController::BpfMode::ForceBand);
    }
};

QTEST_MAIN(TstLocalFilterPolicyPersistence)
#include "tst_local_filter_policy_persistence.moc"
