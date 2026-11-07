// =================================================================
// tests/tst_transmit_model_mic_source_preconnect.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the old pre-connect Mic_Source keys. setMicSource wrote a global
// "tx/preconnect/Mic_Source" key from eager-borg-d64bed (2026-05-06) until
// 2026-11-05, when a pick with no radio joined every other transmit edit in
// the held area. Existing installs still carry the old key, never cleared, so
// loadFromSettings keeps reading it when the per-MAC key is absent, and it
// must never beat a per-MAC value.
//
// Coverage:
//   preconnectClick_isHeldNotWrittenToTheOldKey - Vax click with no radio goes
//                                           to the held area, not the old key
//   load_emptyPerMac_usesPreconnect      - load with no per-MAC value
//                                           picks up the old key
//   load_perMacOverridesPreconnect       - explicit per-MAC value wins
//   perMacWrite_doesNotTouchPreconnect   - post-connect write goes only
//                                           to the per-MAC key
//   load_neitherKey_defaultsToPc         - first-run baseline preserved
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-06 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code (eager-borg-d64bed).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>

#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstTransmitModelMicSourcePreconnect : public QObject {
    Q_OBJECT

private:
    static QString preconnectKey()
    {
        return QStringLiteral("tx/preconnect/Mic_Source");
    }

    static QString heldKey()
    {
        return TransmitModel::heldEditsPrefix() + QStringLiteral("tx/Mic_Source");
    }

    static QString perMacKey(const QString& mac)
    {
        return QStringLiteral("hardware/%1/tx/Mic_Source").arg(mac);
    }

    // Clear per-test state from the AppSettings singleton so the tests
    // are independent of execution order.  Each test uses a unique MAC
    // so per-MAC keys can't collide; the preconnect global key is shared
    // and explicitly cleared here.
    void clearState(const QString& mac)
    {
        auto& s = AppSettings::instance();
        s.remove(preconnectKey());
        s.remove(heldKey());
        s.remove(perMacKey(mac));
    }

private slots:

    // ── 1. A click with no radio is held, not written to the old key ───
    // The per-MAC key must not be touched while no MAC is bound.

    void preconnectClick_isHeldNotWrittenToTheOldKey()
    {
        const QString mac = QStringLiteral("tst-preconnect-aa-bb-cc-dd-ee-01");
        clearState(mac);

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.setKeepsEditsWithoutRadio(true);
        // No loadFromSettings call: m_persistMac is empty.
        m.setMicSource(MicSource::Vax);

        QCOMPARE(AppSettings::instance().value(heldKey()).toString(), QStringLiteral("Vax"));
        QVERIFY(!AppSettings::instance().contains(preconnectKey()));
        QVERIFY(AppSettings::instance().value(perMacKey(mac)).toString().isEmpty());
        AppSettings::instance().remove(heldKey());
    }

    // ── 2. Load with no per-MAC value falls back to preconnect ──────────
    // The first time a radio is connected, its per-MAC Mic_Source key
    // doesn't exist.  Without the fallback, loadFromSettings would default
    // to "Pc" and clobber the user's pre-connect choice.

    void load_emptyPerMac_usesPreconnect()
    {
        const QString mac = QStringLiteral("tst-preconnect-aa-bb-cc-dd-ee-02");
        clearState(mac);

        AppSettings::instance().setValue(preconnectKey(), QStringLiteral("Vax"));

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.loadFromSettings(mac);
        QCOMPARE(m.micSource(), MicSource::Vax);
    }

    // ── 3. Per-MAC value overrides preconnect ───────────────────────────
    // If the user has previously set Mic_Source for THIS specific radio,
    // the per-MAC value must win — the pre-connect fallback is only for
    // first-touch radios, not a global override.

    void load_perMacOverridesPreconnect()
    {
        const QString mac = QStringLiteral("tst-preconnect-aa-bb-cc-dd-ee-03");
        clearState(mac);

        // Pre-connect says Vax, but this specific radio has Pc stored.
        AppSettings::instance().setValue(preconnectKey(), QStringLiteral("Vax"));
        AppSettings::instance().setValue(perMacKey(mac), QStringLiteral("Pc"));

        TransmitModel m;
        m.loadFromSettings(mac);
        QCOMPARE(m.micSource(), MicSource::Pc);
    }

    // ── 4. Post-connect write does not touch preconnect ─────────────────
    // After loadFromSettings binds a MAC, setMicSource should write only
    // to the per-MAC key.  The preconnect fallback represents a one-shot
    // pre-connect intent and must not be silently rewritten.

    void perMacWrite_doesNotTouchPreconnect()
    {
        const QString mac = QStringLiteral("tst-preconnect-aa-bb-cc-dd-ee-04");
        clearState(mac);

        AppSettings::instance().setValue(preconnectKey(), QStringLiteral("Vax"));

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.loadFromSettings(mac);
        // load_emptyPerMac path: m.micSource() is now Vax.
        QCOMPARE(m.micSource(), MicSource::Vax);

        // Now the user picks Pc for this radio.
        m.setMicSource(MicSource::Pc);

        // Preconnect must be untouched.
        QCOMPARE(AppSettings::instance().value(preconnectKey()).toString(),
                 QStringLiteral("Vax"));
        // Per-MAC key gets the new value.
        QCOMPARE(AppSettings::instance().value(perMacKey(mac)).toString(),
                 QStringLiteral("Pc"));
    }

    // ── 5. First-run with no keys at all defaults to Pc ─────────────────
    // Cold start: no preconnect, no per-MAC.  Existing default-Pc
    // behaviour preserved.

    void load_neitherKey_defaultsToPc()
    {
        const QString mac = QStringLiteral("tst-preconnect-aa-bb-cc-dd-ee-05");
        clearState(mac);
        // No preconnect key written; no per-MAC key written.

        TransmitModel m;
        m.loadFromSettings(mac);
        QCOMPARE(m.micSource(), MicSource::Pc);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelMicSourcePreconnect)
#include "tst_transmit_model_mic_source_preconnect.moc"
