// =================================================================
// tests/tst_pan_display_settings_inherit.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Per-pan display settings keys are a
// NereusSDR construct (AetherSDR pattern: "DisplayGridMax" for pan 0,
// "DisplayGridMax_1" for pan 1). Thetis has a fixed RX1/RX2 pair with
// hand-written RX2 duplicates of some settings and no per-pan scheme to
// port.
//
// Bench report 2026-07-30 (JJ, KG4VCF): "the second-N panadapters seem to
// not honour the settings for display, pan 1 does, seems no way to adjust
// the others."
//
// Two causes. Setup pushed every display control through a single
// SpectrumWidget captured once at startup (fixed in MainWindow, covered by
// bench row 27, not reachable from a unit test without a MainWindow). The
// other is here: a pan opening for the first time has no keys of its own,
// so every read fell through to the hardcoded ship defaults and the new
// pan came up looking nothing like the one beside it.
//
// The chosen model (2026-07-30): a pan follows pan 0 until it is given a
// setting of its own, then it is independent. Implemented as a fallback in
// the read path rather than by copying keys, so "untouched" keeps tracking
// pan 0 instead of freezing whatever pan 0 looked like at creation.
//
// NOTE ON ISOLATION: TestSandboxInit redirects the settings path away from
// the operator's profile. This test also snapshots the keys it changes and
// restores their previous in-process values in cleanupTestCase.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;
using namespace Qt::StringLiterals;

namespace {
// Two keys with different read paths: a float through readFloat and a
// bool through readBool. Both must inherit, and they did not share an
// implementation before this change.
const QString kFloatKey  = u"DisplayGridMax"_s;
const QString kFloatKey1 = u"DisplayGridMax_1"_s;
const QString kFillColorKey = u"DisplayFillColor"_s;
const QString kFillColorKey1 = u"DisplayFillColor_1"_s;
const QString kFillColorKey2 = u"DisplayFillColor_2"_s;
const QString kGridColorKey = u"DisplayGridColor"_s;
const QString kGridColorKey1 = u"DisplayGridColor_1"_s;
} // namespace

class TestPanDisplaySettingsInherit : public QObject
{
    Q_OBJECT

private:
    struct Saved { bool existed{false}; QString value; };
    QHash<QString, Saved> m_saved;

    void snapshot(const QString& key)
    {
        auto& s = AppSettings::instance();
        Saved sv;
        sv.value   = s.value(key).toString();
        sv.existed = !sv.value.isEmpty();
        m_saved.insert(key, sv);
    }

private slots:

    void initTestCase()
    {
        snapshot(kFloatKey);
        snapshot(kFloatKey1);
        snapshot(kFillColorKey);
        snapshot(kFillColorKey1);
        snapshot(kFillColorKey2);
        snapshot(kGridColorKey);
        snapshot(kGridColorKey1);
    }

    void cleanupTestCase()
    {
        auto& s = AppSettings::instance();
        for (auto it = m_saved.cbegin(); it != m_saved.cend(); ++it) {
            // Restore, including "was not set": an empty string is what an
            // absent key reads back as throughout this class, and every
            // read path treats empty as absent.
            s.setValue(it.key(), it.value().existed ? it.value().value : QString());
        }
    }

    // ── 1. An untouched pan inherits pan 0 ───────────────────────────────
    //
    // The finding. Before the fix this returned the hardcoded ship default
    // (-48.0) rather than pan 0's value, so a new pan opened looking
    // nothing like the one it was placed beside.
    void an_untouched_pan_reads_pan_zeros_value()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey,  u"-30"_s);   // pan 0 says -30
        s.setValue(kFloatKey1, QString());  // pan 1 has never been set

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();

        // refLevel is m_refLevel, loaded straight from DisplayGridMax.
        QVERIFY2(qFuzzyCompare(pan1.refLevel() + 1.0F, -30.0F + 1.0F),
            qPrintable(u"pan 1 should inherit pan 0's grid max of -30, got %1"_s
                           .arg(static_cast<double>(pan1.refLevel()))));
    }

    // ── 2. Its own value wins once it has one ────────────────────────────
    //
    // The other half of "follow pan 1 until you say otherwise". Without
    // this the fallback would be a permanent link and the per-pan keys
    // that already exist in the settings file would be unreachable.
    void a_pan_with_its_own_value_does_not_inherit()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey,  u"-30"_s);
        s.setValue(kFloatKey1, u"-70"_s);

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();

        QVERIFY2(qFuzzyCompare(pan1.refLevel() + 1.0F, -70.0F + 1.0F),
            qPrintable(u"pan 1 set its own grid max of -70, got %1"_s
                           .arg(static_cast<double>(pan1.refLevel()))));
    }

    // ── 3. Pan 0 does not inherit from itself ────────────────────────────
    //
    // settingsKey(base, 0) returns `base`, so a fallback taken on pan 0
    // would re-read the same key. Harmless today and a trap the moment the
    // key scheme changes, so it is asserted rather than left to luck.
    void pan_zero_reads_its_own_key_only()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey, u"-42"_s);

        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.loadSettings();

        QVERIFY(qFuzzyCompare(pan0.refLevel() + 1.0F, -42.0F + 1.0F));
    }

    // ── 4. Neither set still lands on the ship default ───────────────────
    //
    // First launch, no settings file. The inheritance must not swallow the
    // calibrated defaults, which were tuned against a live ANAN-G2 and are
    // what a new install is judged on.
    void with_nothing_set_the_ship_default_still_applies()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey,  QString());
        s.setValue(kFloatKey1, QString());

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();

        QVERIFY2(qFuzzyCompare(pan1.refLevel() + 1.0F, -48.0F + 1.0F),
            qPrintable(u"expected the -48.0 ship default, got %1"_s
                           .arg(static_cast<double>(pan1.refLevel()))));
    }

    void trace_fill_color_alpha_survives_a_fresh_widget()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFillColorKey, QString());
        const QColor selected(12, 84, 193, 73);

        SpectrumWidget writer;
        writer.setPanIndex(0);
        writer.setFillColor(selected);
        writer.saveSettings();

        QCOMPARE(s.value(kFillColorKey).toString(), selected.name(QColor::HexArgb));
        SpectrumWidget reader;
        reader.setPanIndex(0);
        reader.loadSettings();
        QCOMPARE(reader.fillColor(), selected);
    }

    void trace_fill_color_follows_pan_zero_until_overridden()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFillColorKey, QString());
        s.setValue(kFillColorKey1, QString());
        s.setValue(kFillColorKey2, QString());
        const QColor first(10, 20, 30, 40);
        const QColor second(110, 120, 130, 140);
        const QColor later(200, 190, 180, 170);

        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.setFillColor(first);
        pan0.saveSettings();

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        QCOMPARE(pan1.fillColor(), first);
        QVERIFY(s.value(kFillColorKey1).toString().isEmpty());

        pan1.setFillColor(second);
        pan1.saveSettings();
        QCOMPARE(s.value(kFillColorKey1).toString(), second.name(QColor::HexArgb));
        QCOMPARE(s.value(kFillColorKey).toString(), first.name(QColor::HexArgb));

        pan0.setFillColor(later);
        pan0.saveSettings();
        QCOMPARE(s.value(kFillColorKey1).toString(), second.name(QColor::HexArgb));

        SpectrumWidget independent;
        independent.setPanIndex(1);
        independent.loadSettings();
        QCOMPARE(independent.fillColor(), second);

        SpectrumWidget inheriting;
        inheriting.setPanIndex(2);
        inheriting.loadSettings();
        QCOMPARE(inheriting.fillColor(), later);
        QVERIFY(s.value(kFillColorKey2).toString().isEmpty());
    }

    void trace_fill_color_uses_cyan_for_unset_or_invalid_storage()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFillColorKey, QString());
        s.setValue(kFillColorKey1, QString());

        SpectrumWidget fresh;
        fresh.setPanIndex(1);
        fresh.loadSettings();
        QCOMPARE(fresh.fillColor(), QColor(0x00, 0xe5, 0xff));

        s.setValue(kFillColorKey1, u"invalid-colour"_s);
        SpectrumWidget invalid;
        invalid.setPanIndex(1);
        invalid.loadSettings();
        QCOMPARE(invalid.fillColor(), QColor(0x00, 0xe5, 0xff));
    }

    void existing_grid_color_inherits_pan_zero_but_valid_override_wins()
    {
        auto& s = AppSettings::instance();
        s.setValue(kGridColorKey, QString());
        s.setValue(kGridColorKey1, QString());
        const QColor first(40, 70, 100, 130);
        const QColor second(12, 34, 56, 78);

        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.setGridColor(first);
        pan0.saveSettings();

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        QCOMPARE(pan1.gridColor(), first);
        QVERIFY(s.value(kGridColorKey1).toString().isEmpty());

        pan1.setGridColor(second);
        pan1.saveSettings();
        QCOMPARE(s.value(kGridColorKey1).toString(), second.name(QColor::HexArgb));
        QCOMPARE(s.value(kGridColorKey).toString(), first.name(QColor::HexArgb));
        SpectrumWidget overrideReader;
        overrideReader.setPanIndex(1);
        overrideReader.loadSettings();
        QCOMPARE(overrideReader.gridColor(), second);

        s.setValue(kGridColorKey1, u"invalid-colour"_s);
        SpectrumWidget invalidReader;
        invalidReader.setPanIndex(1);
        invalidReader.loadSettings();
        QCOMPARE(invalidReader.gridColor(), QColor(255, 255, 255, 40));
        QCOMPARE(s.value(kGridColorKey).toString(), first.name(QColor::HexArgb));
    }
};

QTEST_MAIN(TestPanDisplaySettingsInherit)
#include "tst_pan_display_settings_inherit.moc"
