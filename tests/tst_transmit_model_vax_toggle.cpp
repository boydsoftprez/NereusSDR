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
//   persistence_preconnectFallback       - missing per-MAC falls back to preconnect
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>

#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

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
        m.loadFromSettings(mac);
        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);
    }

    void toggleOff_restoresPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-04");
        clearState(mac);

        TransmitModel m;
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
        pre.setMicSource(MicSource::Radio);
        QCOMPARE(AppSettings::instance().value(preconnectPreVaxKey()).toString(),
                 QStringLiteral("Radio"));

        // First connect to this radio: per-MAC PreVax absent, fall back to preconnect.
        TransmitModel m;
        m.loadFromSettings(mac);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelVaxToggle)
#include "tst_transmit_model_vax_toggle.moc"
