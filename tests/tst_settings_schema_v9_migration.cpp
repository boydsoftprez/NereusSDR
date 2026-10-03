// =================================================================
// tests/tst_settings_schema_v9_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R-IOS-06, R-IOS-27: SettingsSchemaVersion v8 -> v9 brings each slice's
// saved NR1 values into Thetis's NR spinbox ranges once, so the VFO flag's
// NR1 popup shows what runs:
//   - a gain or leak exactly equal to the old default (16e-4, 10e-7, which
//     no operator chose) becomes Thetis's default (100 x 1e-6, 100 x 1e-3);
//   - a value outside the range (taps 1-1024, delay 1-1023, gain 1-1000 x
//     1e-6, leak 1-1000 x 1e-3) is clamped into it and written back;
//   - a value the operator set inside the range stays.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-IOS-06, R-IOS-27).
// =================================================================

#include <QtTest/QtTest>

#include <cmath>

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
const QString kVersionKey = QStringLiteral("SettingsSchemaVersion");

double stored(const QString& key)
{
    return AppSettings::instance().value(key).toString().toDouble();
}

bool same(double a, double b)
{
    return std::abs(a - b) <= 1e-15 * std::max(1.0, std::abs(b));
}

void clearSlices()
{
    auto& s = AppSettings::instance();
    for (int i = 0; i < 4; ++i) {
        for (const char* name : {"Nr1Taps", "Nr1Delay", "Nr1Gain", "Nr1Leakage", "AfGain"}) {
            s.remove(QStringLiteral("Slice%1/%2").arg(i).arg(QLatin1String(name)));
        }
    }
    s.remove(kVersionKey);
}
} // namespace

class TestSettingsSchemaV9Migration : public QObject {
    Q_OBJECT
private slots:
    void init() { clearSlices(); }
    void cleanup() { clearSlices(); }

    void v8_to_v9_brings_saved_nr1_values_into_range()
    {
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("8"));
        // Slice A: the old defaults, as SliceModel saved them.
        s.setValue(QStringLiteral("Slice0/Nr1Gain"), QStringLiteral("0.0016"));
        s.setValue(QStringLiteral("Slice0/Nr1Leakage"), QStringLiteral("1e-06"));
        // Slice B: outside the range (the old sliders reached 0; a mirror
        // write could reach anything).
        s.setValue(QStringLiteral("Slice1/Nr1Gain"), QStringLiteral("0"));
        s.setValue(QStringLiteral("Slice1/Nr1Leakage"), QStringLiteral("2.5"));
        s.setValue(QStringLiteral("Slice1/Nr1Taps"), QStringLiteral("2000"));
        s.setValue(QStringLiteral("Slice1/Nr1Delay"), QStringLiteral("0"));
        // Slice C: the operator's own values, inside the range.
        s.setValue(QStringLiteral("Slice2/Nr1Gain"), QStringLiteral("0.00025"));
        s.setValue(QStringLiteral("Slice2/Nr1Leakage"), QStringLiteral("0.05"));
        s.setValue(QStringLiteral("Slice2/Nr1Taps"), QStringLiteral("128"));
        s.setValue(QStringLiteral("Slice2/Nr1Delay"), QStringLiteral("256"));
        s.setValue(QStringLiteral("Slice2/AfGain"), QStringLiteral("0"));

        s.ensureSettingsAtVersion(9);

        using namespace ControlRanges;
        // All the rewritten values at once, so a failure shows each of them.
        const auto summary = [](double g0, double l0, double g1, double l1, double t1,
                                double d1) {
            return QStringLiteral("old defaults: gain %1 leak %2; out of range: gain %3 "
                                  "leak %4 taps %5 delay %6")
                .arg(g0, 0, 'g', 6).arg(l0, 0, 'g', 6).arg(g1, 0, 'g', 6)
                .arg(l1, 0, 'g', 6).arg(t1).arg(d1);
        };
        QCOMPARE(summary(stored(QStringLiteral("Slice0/Nr1Gain")),
                         stored(QStringLiteral("Slice0/Nr1Leakage")),
                         stored(QStringLiteral("Slice1/Nr1Gain")),
                         stored(QStringLiteral("Slice1/Nr1Leakage")),
                         stored(QStringLiteral("Slice1/Nr1Taps")),
                         stored(QStringLiteral("Slice1/Nr1Delay"))),
                 summary(kNr1Gain.defaultValue, kNr1Leak.defaultValue,
                         kNr1Gain.min * kNr1Gain.scale, kNr1Leak.max * kNr1Leak.scale,
                         kNr1Taps.max, kNr1Delay.min));
        QVERIFY(same(stored(QStringLiteral("Slice0/Nr1Gain")), kNr1Gain.defaultValue));
        QVERIFY(same(stored(QStringLiteral("Slice0/Nr1Leakage")), kNr1Leak.defaultValue));

        QVERIFY(same(stored(QStringLiteral("Slice1/Nr1Gain")), kNr1Gain.min * kNr1Gain.scale));
        QVERIFY(same(stored(QStringLiteral("Slice1/Nr1Leakage")), kNr1Leak.max * kNr1Leak.scale));
        QCOMPARE(stored(QStringLiteral("Slice1/Nr1Taps")), 1024.0);
        QCOMPARE(stored(QStringLiteral("Slice1/Nr1Delay")), 1.0);

        QCOMPARE(s.value(QStringLiteral("Slice2/Nr1Gain")).toString(), QStringLiteral("0.00025"));
        QCOMPARE(s.value(QStringLiteral("Slice2/Nr1Leakage")).toString(), QStringLiteral("0.05"));
        QCOMPARE(s.value(QStringLiteral("Slice2/Nr1Taps")).toString(), QStringLiteral("128"));
        QCOMPARE(s.value(QStringLiteral("Slice2/Nr1Delay")).toString(), QStringLiteral("256"));
        QCOMPARE(s.value(QStringLiteral("Slice2/AfGain")).toString(), QStringLiteral("0"));
        QCOMPARE(s.value(kVersionKey).toString(), QStringLiteral("9"));

        // What a slice loads is inside the range the popup's sliders span,
        // so the popup shows what runs.
        for (int i = 0; i < 3; ++i) {
            SliceModel slice(i);
            slice.restoreFromSettings(Band::Band20m);
            for (const auto& [control, value] :
                 {std::pair{kNr1Taps, double(slice.nr1Taps())},
                  std::pair{kNr1Delay, double(slice.nr1Delay())},
                  std::pair{kNr1Gain, slice.nr1Gain()},
                  std::pair{kNr1Leak, slice.nr1Leakage()}}) {
                const int position = nrSliderFromValue(control, value);
                QVERIFY2(position >= control.min && position <= control.max,
                         qPrintable(QStringLiteral("slice %1 %2 at %3")
                                        .arg(i).arg(QLatin1String(control.property))
                                        .arg(position)));
            }
        }
    }

    void v9_keeps_what_was_chosen_after_the_migration()
    {
        // It runs once: the old default chosen again on a v9 file stays.
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("9"));
        s.setValue(QStringLiteral("Slice0/Nr1Gain"), QStringLiteral("0.0016"));

        s.ensureSettingsAtVersion(9);

        QCOMPARE(s.value(QStringLiteral("Slice0/Nr1Gain")).toString(), QStringLiteral("0.0016"));
    }
};

QTEST_MAIN(TestSettingsSchemaV9Migration)
#include "tst_settings_schema_v9_migration.moc"
