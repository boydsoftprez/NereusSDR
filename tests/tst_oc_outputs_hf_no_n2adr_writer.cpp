// no-port-check: NereusSDR-original test for issue #174 cleanup.
//
// Issue #174: the OcOutputsHfTab "N2ADR Filter (HERCULES)" checkbox was
// removed because it wrote to a global "hardware/oc/n2adrFilter" key
// that had no consumer.  The actual N2ADR setting lives at per-MAC
// hardware/<mac>/hl2IoBoard/n2adrFilter (Hl2IoBoardTab + RadioModel
// connect-time reconcile).
//
// This is a tombstone test — it verifies:
//   1. AppSettings::removeOrphanOcN2adrFilter clears the global key,
//      logs once, and is idempotent.
//   2. Constructing OcOutputsHfTab does NOT (re-)write the orphan key.
//
// If a future regression brings the dead checkbox back, both checks will
// fail loudly.

#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/OcMatrix.h"
#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstOcOutputsHfNoN2adrWriter : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // The orphan-key cleanup removes the legacy global key.
    // ─────────────────────────────────────────────────────────────────────
    void removeOrphan_clears_present_key()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        s.setValue(QStringLiteral("hardware/oc/n2adrFilter"),
                   QStringLiteral("True"));
        QVERIFY(s.contains(QStringLiteral("hardware/oc/n2adrFilter")));

        AppSettings::removeOrphanOcN2adrFilter(s);

        QVERIFY2(!s.contains(QStringLiteral("hardware/oc/n2adrFilter")),
                 "issue #174: orphan hardware/oc/n2adrFilter key was not "
                 "removed by removeOrphanOcN2adrFilter()");
    }

    // ─────────────────────────────────────────────────────────────────────
    // Idempotent: a second call after the key is gone is a safe no-op.
    // ─────────────────────────────────────────────────────────────────────
    void removeOrphan_is_idempotent()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        AppSettings::removeOrphanOcN2adrFilter(s);  // key absent
        AppSettings::removeOrphanOcN2adrFilter(s);  // still absent

        QVERIFY(!s.contains(QStringLiteral("hardware/oc/n2adrFilter")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Persists across save/reload — proves we hit the on-disk file too.
    // ─────────────────────────────────────────────────────────────────────
    void removeOrphan_persists_across_reload()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("hardware/oc/n2adrFilter"),
                       QStringLiteral("False"));
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QVERIFY(s.contains(QStringLiteral("hardware/oc/n2adrFilter")));
            AppSettings::removeOrphanOcN2adrFilter(s);
        }
        {
            AppSettings s2(path);
            s2.load();
            QVERIFY(!s2.contains(QStringLiteral("hardware/oc/n2adrFilter")));
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Constructing OcOutputsHfTab must not (re-)write the orphan key —
    // catches a regression that brings the dead checkbox back.
    // Uses the AppSettings singleton because OcOutputsHfTab writes through
    // it; we snapshot before / after construction.
    // ─────────────────────────────────────────────────────────────────────
    void constructing_tab_does_not_write_orphan_key()
    {
        // Make sure the singleton starts clean.
        AppSettings::instance().remove(
            QStringLiteral("hardware/oc/n2adrFilter"));
        QVERIFY(!AppSettings::instance().contains(
            QStringLiteral("hardware/oc/n2adrFilter")));

        RadioModel model;
        OcMatrix oc;
        OcOutputsHfTab tab(&model, &oc);
        Q_UNUSED(tab);

        QVERIFY2(!AppSettings::instance().contains(
                     QStringLiteral("hardware/oc/n2adrFilter")),
                 "issue #174 regression: OcOutputsHfTab construction wrote "
                 "the orphan hardware/oc/n2adrFilter key — the dead "
                 "checkbox is back");
    }
};

QTEST_MAIN(TstOcOutputsHfNoN2adrWriter)
#include "tst_oc_outputs_hf_no_n2adr_writer.moc"
