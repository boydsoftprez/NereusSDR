// =================================================================
// tests/tst_transmit_model_vax_toggle.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the PhoneCwApplet VAX-button toggle plumbing on
// TransmitModel: previous non-VAX source tracking, toggleVaxSource
// slot, HL2 lock interaction, and Mic_Source_PreVax persistence
// (per-MAC + preconnect fallback).
//
// Coverage:
//   defaults_previousIsPc                - cold construct -> Pc default
//   setMicSource_radioTracksPrevious     - explicit Radio captured as previous
//   setMicSource_vaxDoesNotTrackPrevious - Vax write does NOT clobber previous
//   toggleOn_setsVax                     - toggleVaxSource(true) -> Vax
//   toggleOff_restoresPrevious           - toggleVaxSource(false) -> previous
//   toggleOff_hl2LockedRestoresPc        - lock active, previous=Radio coerced
//   persistence_perMacRoundTrip          - PreVax key round-trips per-MAC
//   persistence_preconnectFallback       - a pick with no radio reaches the first radio
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-10-09 - VAX TX availability per system (R-SPK-21, R-AUD-01).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-10-14 - Regress stale restore keys overriding a loaded source,
//                 including preconnect fallback and reload while VAX is
//                 active. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QScopeGuard>

#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

// MinGW imports a static constexpr member of an exported class by address
// when it is bound to a reference (QCOMPARE does), and the Core DLL never
// exports one, so this test did not link on Windows. It reads these
// copies by value instead.
namespace {
constexpr bool kVaxSourceAvailableOnThisSystem = NereusSDR::TransmitModel::kVaxSourceAvailableOnThisSystem;
} // namespace

class TstTransmitModelVaxToggle : public QObject {
    Q_OBJECT

private:
    static QString preconnectMicKey()
    {
        return QStringLiteral("tx/preconnect/Mic_Source");
    }
    static QString preconnectPreVaxKey()
    {
        return QStringLiteral("tx/preconnect/Mic_Source_PreVax");
    }
    static QString perMacMicKey(const QString& mac)
    {
        return QStringLiteral("hardware/%1/tx/Mic_Source").arg(mac);
    }
    static QString perMacPreVaxKey(const QString& mac)
    {
        return QStringLiteral("hardware/%1/tx/Mic_Source_PreVax").arg(mac);
    }

    void clearState(const QString& mac)
    {
        auto& s = AppSettings::instance();
        s.remove(preconnectMicKey());
        s.remove(preconnectPreVaxKey());
        s.remove(TransmitModel::heldEditsPrefix() + QStringLiteral("tx/Mic_Source"));
        s.remove(TransmitModel::heldEditsPrefix() + QStringLiteral("tx/Mic_Source_PreVax"));
        s.remove(perMacMicKey(mac));
        s.remove(perMacPreVaxKey(mac));
    }

private slots:

    void defaults_previousIsPc()
    {
        TransmitModel m;
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Pc);
    }

    void setMicSource_radioTracksPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-01");
        clearState(mac);

        TransmitModel m;
        m.loadFromSettings(mac);
        m.setMicSource(MicSource::Radio);

        QCOMPARE(m.micSource(), MicSource::Radio);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
    }

    void setMicSource_vaxDoesNotTrackPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-02");
        clearState(mac);

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.loadFromSettings(mac);
        m.setMicSource(MicSource::Radio);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);

        m.setMicSource(MicSource::Vax);
        // Previous stays Radio. Toggling off should land back on Radio.
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
    }

    void toggleOn_setsVax()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-03");
        clearState(mac);

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.loadFromSettings(mac);
        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);
    }

    void toggleOff_restoresPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-04");
        clearState(mac);

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.loadFromSettings(mac);
        m.setMicSource(MicSource::Radio);
        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);

        m.toggleVaxSource(false);
        QCOMPARE(m.micSource(), MicSource::Radio);
    }

    void toggleOff_hl2LockedRestoresPc()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-05");
        clearState(mac);

        TransmitModel m;
        // Tests the VAX path itself, which Windows does not offer.
        m.setVaxSourceAvailable(true);
        m.loadFromSettings(mac);
        // Pretend Radio was the previous source on a non-HL2 radio.
        m.setMicSource(MicSource::Radio);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);

        // Switch to HL2: lock engages, current Radio coerces to Pc.
        m.setMicSourceLocked(true);
        QCOMPARE(m.micSource(), MicSource::Pc);
        // The lock-coerce write also updated previous to Pc.
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Pc);

        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);
        m.toggleVaxSource(false);
        QCOMPARE(m.micSource(), MicSource::Pc);
    }

    void persistence_perMacRoundTrip()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-06");
        clearState(mac);

        {
            TransmitModel m;
            m.loadFromSettings(mac);
            m.setMicSource(MicSource::Radio);  // captures Radio as previous + persists
            QCOMPARE(AppSettings::instance().value(perMacPreVaxKey(mac)).toString(),
                     QStringLiteral("Radio"));
        }

        {
            TransmitModel m;
            m.loadFromSettings(mac);
            QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
        }
    }

    void persistence_preconnectFallback()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-07");
        clearState(mac);

        // User picks Radio in Setup before connecting (no MAC bound).
        TransmitModel pre;
        pre.setKeepsEditsWithoutRadio(true);
        pre.setMicSource(MicSource::Radio);
        QCOMPARE(AppSettings::instance()
                     .value(TransmitModel::heldEditsPrefix()
                            + QStringLiteral("tx/Mic_Source_PreVax"))
                     .toString(),
                 QStringLiteral("Radio"));
        QVERIFY(!AppSettings::instance().contains(preconnectPreVaxKey()));

        // First connect to this radio: the held choice is now this radio's.
        TransmitModel m;
        m.loadFromSettings(mac);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
        QVERIFY(!AppSettings::instance().contains(
            TransmitModel::heldEditsPrefix() + QStringLiteral("tx/Mic_Source_PreVax")));
    }

    // A stale restore key must not override the source actually selected
    // for this radio, including when the setter sees an unchanged source.
    void load_nonVaxChoiceWinsOverStaleRestore_data()
    {
        QTest::addColumn<MicSource>("selected");
        QTest::addColumn<QString>("selectedText");
        QTest::addColumn<QString>("staleText");
        QTest::addColumn<bool>("perMacRestore");
        QTest::addColumn<bool>("perMacChoice");
        QTest::newRow("per-mac-pc-stale-radio")
            << MicSource::Pc << QStringLiteral("Pc") << QStringLiteral("Radio") << true << true;
        QTest::newRow("per-mac-pc-stale-preconnect-radio")
            << MicSource::Pc << QStringLiteral("Pc") << QStringLiteral("Radio") << false << true;
        QTest::newRow("per-mac-radio-stale-pc")
            << MicSource::Radio << QStringLiteral("Radio") << QStringLiteral("Pc") << true << true;
        QTest::newRow("per-mac-radio-stale-preconnect-pc")
            << MicSource::Radio << QStringLiteral("Radio") << QStringLiteral("Pc") << false << true;
        QTest::newRow("preconnect-pc-stale-radio")
            << MicSource::Pc << QStringLiteral("Pc") << QStringLiteral("Radio") << false << false;
        QTest::newRow("preconnect-radio-stale-pc")
            << MicSource::Radio << QStringLiteral("Radio") << QStringLiteral("Pc") << false << false;
    }

    void load_nonVaxChoiceWinsOverStaleRestore()
    {
        QFETCH(MicSource, selected);
        QFETCH(QString, selectedText);
        QFETCH(QString, staleText);
        QFETCH(bool, perMacRestore);
        QFETCH(bool, perMacChoice);
        const QString mac = QStringLiteral("tst-vaxtog-stale-restore");
        clearState(mac);
        const auto cleanup = qScopeGuard([&] { clearState(mac); });
        TransmitModel model;
        model.setVaxSourceAvailable(true);
        model.setMicSource(selected);
        auto& settings = AppSettings::instance();
        settings.setValue(preconnectMicKey(), perMacChoice ? staleText : selectedText);
        settings.setValue(preconnectPreVaxKey(), staleText);
        if (perMacChoice) {
            settings.setValue(perMacMicKey(mac), selectedText);
        }
        if (perMacRestore) {
            settings.setValue(perMacPreVaxKey(mac), staleText);
        }

        model.loadFromSettings(mac);
        QCOMPARE(model.micSource(), selected);
        model.toggleVaxSource(true);
        QCOMPARE(model.micSource(), MicSource::Vax);
        model.toggleVaxSource(false);
        QCOMPARE(model.micSource(), selected);

        model.persistToSettings(mac);
        QCOMPARE(settings.value(perMacMicKey(mac)).toString(), selectedText);
        QCOMPARE(settings.value(perMacPreVaxKey(mac)).toString(), selectedText);
        TransmitModel reloaded;
        reloaded.setVaxSourceAvailable(true);
        reloaded.loadFromSettings(mac);
        reloaded.toggleVaxSource(true);
        reloaded.toggleVaxSource(false);
        QCOMPARE(reloaded.micSource(), selected);
    }

    void load_nonVaxChoiceSurvivesReloadWhileVax()
    {
        const QString mac = QStringLiteral("tst-vaxtog-reload-while-vax");
        clearState(mac);
        const auto cleanup = qScopeGuard([&] { clearState(mac); });
        auto& settings = AppSettings::instance();
        settings.setValue(perMacMicKey(mac), QStringLiteral("Pc"));
        settings.setValue(preconnectPreVaxKey(), QStringLiteral("Radio"));
        TransmitModel model;
        model.setVaxSourceAvailable(true);
        model.loadFromSettings(mac);
        model.toggleVaxSource(true);
        QCOMPARE(settings.value(perMacMicKey(mac)).toString(), QStringLiteral("Vax"));

        // Source setters auto-persist; reload those values while VAX is
        // active, before a later full-model save can repair stale state.
        TransmitModel reloaded;
        reloaded.setVaxSourceAvailable(true);
        reloaded.loadFromSettings(mac);
        QCOMPARE(reloaded.micSource(), MicSource::Vax);
        reloaded.toggleVaxSource(false);
        QCOMPARE(reloaded.micSource(), MicSource::Pc);
    }

    void load_savedVaxRetainsRestoreChoice_data()
    {
        QTest::addColumn<QString>("perMacRestore");
        QTest::addColumn<QString>("preconnectRestore");
        QTest::addColumn<MicSource>("expected");
        QTest::newRow("per-mac-radio-beats-global-pc")
            << QStringLiteral("Radio") << QStringLiteral("Pc") << MicSource::Radio;
        QTest::newRow("per-mac-pc-beats-global-radio")
            << QStringLiteral("Pc") << QStringLiteral("Radio") << MicSource::Pc;
        QTest::newRow("preconnect-radio-fallback")
            << QString() << QStringLiteral("Radio") << MicSource::Radio;
        QTest::newRow("missing-restore-defaults-to-pc")
            << QString() << QString() << MicSource::Pc;
        QTest::newRow("invalid-restore-defaults-to-pc")
            << QStringLiteral("Vax") << QStringLiteral("Radio") << MicSource::Pc;
    }

    void load_savedVaxRetainsRestoreChoice()
    {
        QFETCH(QString, perMacRestore);
        QFETCH(QString, preconnectRestore);
        QFETCH(MicSource, expected);
        const QString mac = QStringLiteral("tst-vaxtog-saved-vax-restore");
        clearState(mac);
        const auto cleanup = qScopeGuard([&] { clearState(mac); });
        auto& settings = AppSettings::instance();
        settings.setValue(perMacMicKey(mac), QStringLiteral("Vax"));
        settings.setValue(preconnectMicKey(), QStringLiteral("Pc"));
        if (!perMacRestore.isEmpty()) {
            settings.setValue(perMacPreVaxKey(mac), perMacRestore);
        }
        if (!preconnectRestore.isEmpty()) {
            settings.setValue(preconnectPreVaxKey(), preconnectRestore);
        }

        TransmitModel model;
        model.setVaxSourceAvailable(true);
        model.loadFromSettings(mac);
        QCOMPARE(model.micSource(), MicSource::Vax);
        model.persistToSettings(mac);
        TransmitModel reloaded;
        reloaded.setVaxSourceAvailable(true);
        reloaded.loadFromSettings(mac);
        QCOMPARE(reloaded.micSource(), MicSource::Vax);
        reloaded.toggleVaxSource(false);
        QCOMPARE(reloaded.micSource(), expected);
    }

    // R-SPK-21, R-AUD-01: VAX TX exists on macOS and Linux only.
    void vaxAvailable_followsThisSystem()
    {
        TransmitModel m;
#if defined(Q_OS_WIN)
        QVERIFY(!m.vaxSourceAvailable());
#else
        QVERIFY(m.vaxSourceAvailable());
#endif
        QCOMPARE(m.vaxSourceAvailable(), kVaxSourceAvailableOnThisSystem);
    }

    // Without a VAX transmit device, a VAX pick and the VAX toggle fall
    // back to Pc, as the HL2 lock does for Radio.
    void vaxUnavailable_pickFallsBackToPc()
    {
        TransmitModel m;
        m.setMicSource(MicSource::Radio);
        m.setVaxSourceAvailable(false);
        m.setMicSource(MicSource::Vax);
        QCOMPARE(m.micSource(), MicSource::Pc);
        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Pc);
    }

    // A VAX choice already active falls back to Pc when it becomes
    // unavailable; a saved VAX choice loads as Pc.
    void vaxUnavailable_activeAndSavedFallBackToPc()
    {
        TransmitModel active;
        // Starts from an active VAX choice, which Windows does not offer.
        active.setVaxSourceAvailable(true);
        active.setMicSource(MicSource::Vax);
        QCOMPARE(active.micSource(), MicSource::Vax);
        active.setVaxSourceAvailable(false);
        QCOMPARE(active.micSource(), MicSource::Pc);

        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-08");
        clearState(mac);
        AppSettings::instance().setValue(perMacMicKey(mac), QStringLiteral("Vax"));
        AppSettings::instance().setValue(perMacPreVaxKey(mac), QStringLiteral("Radio"));
        TransmitModel saved;
        saved.setVaxSourceAvailable(false);
        saved.loadFromSettings(mac);
        QCOMPARE(saved.micSource(), MicSource::Pc);
        saved.toggleVaxSource(true);
        saved.toggleVaxSource(false);
        QCOMPARE(saved.micSource(), MicSource::Pc);
        clearState(mac);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelVaxToggle)
#include "tst_transmit_model_vax_toggle.moc"
