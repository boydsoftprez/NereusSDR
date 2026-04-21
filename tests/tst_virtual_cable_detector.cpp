// tests/tst_virtual_cable_detector.cpp (NereusSDR)
// Unit tests for VirtualCableDetector: matchProduct() product-regex hook
// (Sub-Phase 7), plus filterThirdParty / fingerprintCsv / diffNewCables
// helpers used by the MainWindow first-run hook (Sub-Phase 11 Task 11b).
// NereusSDR-original; no Thetis/AetherSDR port.
#include <QtTest/QtTest>
#include "core/audio/VirtualCableDetector.h"

using namespace NereusSDR;

namespace {

// Builds a DetectedCable with only the fields the helpers under test
// care about. product differentiates NereusSdrVax from 3rd-party; the
// fingerprint/diff helpers hash deviceName only.
DetectedCable makeCable(VirtualCableProduct p, const QString& name)
{
    return DetectedCable{p, name, true, 0};
}

} // namespace

class TstVirtualCableDetector : public QObject {
    Q_OBJECT
private slots:
    // ── matchProduct() — Sub-Phase 7 regression suite ─────────────────
    void detectsVbCableBase() {
        QVERIFY(VirtualCableDetector::matchProduct(
            "CABLE Input (VB-Audio Virtual Cable)") == VirtualCableProduct::VbCable);
    }
    void detectsVbCableAB() {
        QVERIFY(VirtualCableDetector::matchProduct("CABLE-A Input") == VirtualCableProduct::VbCableA);
        QVERIFY(VirtualCableDetector::matchProduct("CABLE-B Input") == VirtualCableProduct::VbCableB);
    }
    void detectsVac() {
        QVERIFY(VirtualCableDetector::matchProduct("Line 1 (Virtual Audio Cable)")
                == VirtualCableProduct::MuzychenkoVac);
    }
    void detectsVoicemeeter() {
        QVERIFY(VirtualCableDetector::matchProduct("VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)")
                == VirtualCableProduct::Voicemeeter);
    }
    void detectsDante() {
        QVERIFY(VirtualCableDetector::matchProduct("Dante Tx 01-02")
                == VirtualCableProduct::Dante);
    }
    void detectsFlexDax() {
        QVERIFY(VirtualCableDetector::matchProduct("DAX Audio RX 1")
                == VirtualCableProduct::FlexRadioDax);
    }
    void detectsReservedNereusVax() {
        QVERIFY(VirtualCableDetector::matchProduct("NereusSDR VAX 1")
                == VirtualCableProduct::NereusSdrVax);
    }
    void unknownDeviceIsNone() {
        QVERIFY(VirtualCableDetector::matchProduct("Realtek HD Audio Output")
                == VirtualCableProduct::None);
    }
    // Extra: verify CABLE-A prefix doesn't shadow CABLE-AB (shouldn't match VbCableA)
    void cableAbDoesNotMatchCableA() {
        // "CABLE-AB Input" doesn't start with "^CABLE-A " (note trailing space in rule),
        // so it falls through to "^CABLE " and matches VbCable base, NOT VbCableA.
        const auto p = VirtualCableDetector::matchProduct("CABLE-AB Input");
        QVERIFY(p != VirtualCableProduct::VbCableA);
    }

    // ── vendorDisplayName() — product enum → display string ────────────
    void vendorDisplayNameVbCable() {
        QCOMPARE(VirtualCableDetector::vendorDisplayName(VirtualCableProduct::VbCable),
                 QStringLiteral("VB-Audio"));
    }
    void vendorDisplayNameVbCableVariants() {
        // All VB-Audio variants map to the same vendor string.
        QCOMPARE(VirtualCableDetector::vendorDisplayName(VirtualCableProduct::VbCableA),
                 QStringLiteral("VB-Audio"));
        QCOMPARE(VirtualCableDetector::vendorDisplayName(VirtualCableProduct::VbHiFiCable),
                 QStringLiteral("VB-Audio"));
    }
    void vendorDisplayNameFlexDax() {
        QCOMPARE(VirtualCableDetector::vendorDisplayName(VirtualCableProduct::FlexRadioDax),
                 QStringLiteral("FlexRadio DAX"));
    }
    void vendorDisplayNameNoneIsEmpty() {
        QVERIFY(VirtualCableDetector::vendorDisplayName(VirtualCableProduct::None).isEmpty());
    }

    // ── filterThirdParty() — drops NereusSdrVax entries ────────────────
    void filterThirdPartyKeepsVendors() {
        QVector<DetectedCable> all;
        all.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        all.push_back(makeCable(VirtualCableProduct::NereusSdrVax, "NereusSDR VAX 1"));
        all.push_back(makeCable(VirtualCableProduct::Voicemeeter, "VoiceMeeter Input"));
        all.push_back(makeCable(VirtualCableProduct::NereusSdrVax, "NereusSDR VAX 2"));

        const auto filtered = VirtualCableDetector::filterThirdParty(all);
        QCOMPARE(filtered.size(), 2);
        QCOMPARE(filtered[0].product, VirtualCableProduct::VbCableA);
        QCOMPARE(filtered[1].product, VirtualCableProduct::Voicemeeter);
    }
    void filterThirdPartyEmptyIsEmpty() {
        const auto filtered =
            VirtualCableDetector::filterThirdParty(QVector<DetectedCable>{});
        QVERIFY(filtered.isEmpty());
    }
    void filterThirdPartyAllNereusIsEmpty() {
        QVector<DetectedCable> all;
        all.push_back(makeCable(VirtualCableProduct::NereusSdrVax, "NereusSDR VAX 1"));
        all.push_back(makeCable(VirtualCableProduct::NereusSdrVax, "NereusSDR VAX 2"));
        const auto filtered = VirtualCableDetector::filterThirdParty(all);
        QVERIFY(filtered.isEmpty());
    }

    // ── fingerprintCsv() — determinism + format ─────────────────────────
    void fingerprintEmpty() {
        const QString csv =
            VirtualCableDetector::fingerprintCsv(QVector<DetectedCable>{});
        QCOMPARE(csv, QString());
    }
    void fingerprintDeterministicAcrossCalls() {
        QVector<DetectedCable> cables;
        cables.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        cables.push_back(makeCable(VirtualCableProduct::VbCableB, "CABLE-B Input"));

        const QString a = VirtualCableDetector::fingerprintCsv(cables);
        const QString b = VirtualCableDetector::fingerprintCsv(cables);
        QCOMPARE(a, b);
    }
    void fingerprintDifferentInputsDiffer() {
        QVector<DetectedCable> one;
        one.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));

        QVector<DetectedCable> two;
        two.push_back(makeCable(VirtualCableProduct::VbCableB, "CABLE-B Input"));

        QVERIFY(VirtualCableDetector::fingerprintCsv(one)
                != VirtualCableDetector::fingerprintCsv(two));
    }
    void fingerprintCsvFormat() {
        QVector<DetectedCable> cables;
        cables.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        cables.push_back(makeCable(VirtualCableProduct::VbCableB, "CABLE-B Input"));
        cables.push_back(makeCable(VirtualCableProduct::VbCableC, "CABLE-C Input"));

        const QString csv = VirtualCableDetector::fingerprintCsv(cables);
        const QStringList parts = csv.split(QLatin1Char(','));
        QCOMPARE(parts.size(), 3);
        QVERIFY(!csv.endsWith(QLatin1Char(',')));
        for (const auto& p : parts) {
            QCOMPARE(p.size(), 8);
            // 8 lowercase hex chars.
            for (QChar ch : p) {
                const bool isHex = (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                                   || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'));
                QVERIFY(isHex);
            }
        }
    }
    // Algorithm pin: if someone swaps SHA-256 for MD5 or changes the
    // truncation, this test fails. Expected value generated via:
    //   printf 'CABLE-A Input (VB-Audio Virtual Cable)' | shasum -a 256 | cut -c1-8
    void fingerprintAlgorithmPin() {
        QVector<DetectedCable> cables;
        cables.push_back(makeCable(VirtualCableProduct::VbCableA,
                                   "CABLE-A Input (VB-Audio Virtual Cable)"));
        const QString csv = VirtualCableDetector::fingerprintCsv(cables);
        QCOMPARE(csv, QStringLiteral("316ce8dc"));
    }

    // ── diffNewCables() — detects novelty ───────────────────────────────
    void diffAgainstEmptyReturnsAll() {
        QVector<DetectedCable> current;
        current.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        const auto fresh = VirtualCableDetector::diffNewCables(current, QString());
        QCOMPARE(fresh.size(), 1);
        QCOMPARE(fresh[0].deviceName, QStringLiteral("CABLE-A Input"));
    }
    void diffAgainstSelfReturnsEmpty() {
        QVector<DetectedCable> current;
        current.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        current.push_back(makeCable(VirtualCableProduct::VbCableB, "CABLE-B Input"));
        const QString csv = VirtualCableDetector::fingerprintCsv(current);
        const auto fresh = VirtualCableDetector::diffNewCables(current, csv);
        QVERIFY(fresh.isEmpty());
    }
    void diffReturnsOnlyNovel() {
        QVector<DetectedCable> prev;
        prev.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        prev.push_back(makeCable(VirtualCableProduct::VbCableB, "CABLE-B Input"));
        const QString prevCsv = VirtualCableDetector::fingerprintCsv(prev);

        QVector<DetectedCable> current = prev;
        current.push_back(makeCable(VirtualCableProduct::VbCableC, "CABLE-C Input"));

        const auto fresh = VirtualCableDetector::diffNewCables(current, prevCsv);
        QCOMPARE(fresh.size(), 1);
        QCOMPARE(fresh[0].deviceName, QStringLiteral("CABLE-C Input"));
    }
    void diffIgnoresRemovedCables() {
        QVector<DetectedCable> prev;
        prev.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));
        prev.push_back(makeCable(VirtualCableProduct::VbCableB, "CABLE-B Input"));
        const QString prevCsv = VirtualCableDetector::fingerprintCsv(prev);

        // User uninstalled CABLE-B — current has only CABLE-A. No new cables.
        QVector<DetectedCable> current;
        current.push_back(makeCable(VirtualCableProduct::VbCableA, "CABLE-A Input"));

        const auto fresh = VirtualCableDetector::diffNewCables(current, prevCsv);
        QVERIFY(fresh.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstVirtualCableDetector)
#include "tst_virtual_cable_detector.moc"
