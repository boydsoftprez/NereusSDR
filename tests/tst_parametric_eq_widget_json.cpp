// =================================================================
// tests/tst_parametric_eq_widget_json.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.  Cites for the JSON
// marshal logic under test live in ParametricEqWidget.cpp /
// ucParametricEq.cs:1460-1573 [v2.10.3.13].
// =================================================================
//
// Phase 3M-3a-ii follow-up Batch 5 -- JSON marshal verification.  Pins:
//   * saveToJson emits Thetis Newtonsoft snake_case keys.
//   * Point objects deliberately exclude band_id / band_color (Thetis
//     EqJsonPoint contract -- ucParametricEq.cs:242-252).
//   * Decimal precision verbatim: freq=3, gain=1, q=2 (Math.Round in C#).
//   * loadFromJson round-trip preserves freq/gain/q + the global gain /
//     parametric-EQ flag / freq min-max envelope.
//   * loadFromJson clamps each point's freq/gain/q against the loaded
//     freq/gain/Q ranges.
//   * loadFromJson rejects garbage / missing-required-fields / out-of-
//     range band counts / inverted freq ranges (returns false, leaves
//     state unchanged).
//   * loadFromJson does NOT bail on an active drag (matches Thetis,
//     which simply reorders mid-drag via enforceOrdering(true)).
//   * Hand-crafted JSON fixtures pin schema and parsing edge cases.
//
// Hand-computed expected values trace each branch back to the C#
// source in ucParametricEq.cs [v2.10.3.13].
//
// =================================================================

#include "../src/gui/widgets/ParametricEqWidget.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVector>

// Tester shim -- friended in ParametricEqWidget.h so this subclass can
// reach private state for hand-computed expecteds and dragging-state setup.
class ParametricEqJsonTester : public NereusSDR::ParametricEqWidget {
public:
    using NereusSDR::ParametricEqWidget::ParametricEqWidget;

    // Direct member access for assertions.
    QVector<EqPoint>& pointsMut()                { return m_points; }
    const QVector<EqPoint>& pointsConst() const  { return m_points; }
    int    pointCount()                  const   { return m_points.size(); }
    bool   draggingPoint()               const   { return m_draggingPoint; }
    bool   draggingGlobalGain()          const   { return m_draggingGlobalGain; }

    // Force a known band lineup for round-trip tests.  The widget ctor
    // uses resetPointsDefault() for an even spread; this overrides each
    // point's gain/q so we can reason about hand-computed JSON.  We do
    // NOT mutate per-point frequencyHz here -- enforceOrdering(true) on
    // load runs the 5 Hz min-spacing forced-spacing sweep, which would
    // shift any out-of-order interior frequency.  The default 10-band
    // even spread is already in-order, so freq round-trips cleanly.
    void seedBandsLinear() {
        QCOMPARE(m_points.size(), 10);
        m_points[0].gainDb = 5.49;        // -> 5.5 after Math.Round(_, 1)
        m_points[1].gainDb = -3.12;       // -> -3.1
        m_points[2].q       = 4.123;      // -> 4.12 after Math.Round(_, 2)
        m_points[3].q       = 0.998;      // -> 1.00 -- still > qMin so no clamp.
        // freq stays at default 0..4000 even spread (in-order).
    }

    // Force an "active drag" state so loadFromJsonDuringDragMatchesThetis
    // can verify the Thetis-faithful path (which does not bail).
    void forceDraggingPointActive() {
        m_draggingPoint = true;
        m_dragIndex     = 1;
    }

    // Direct write-through for stuffing pre-load state to verify
    // loadFromJson restores the loaded values.
    void setGlobalGainDirect(double gain) { m_globalGainDb = gain; }
};

class TestParametricEqJson : public QObject {
    Q_OBJECT
private slots:
    void saveToJsonProducesSnakeCaseKeys();
    void saveToJsonOmitsBandIdAndBandColor();
    void saveToJsonRoundsFreqToThreeDecimals();
    void saveToJsonRoundsGainToOneDecimal();
    void saveToJsonRoundsQToTwoDecimals();
    void loadFromJsonRoundTripPreservesData();
    void loadFromJsonRetainsExistingBandIds();
    void loadFromJsonRejectsInvalidJson();
    void loadFromJsonRejectsMissingPoints();
    void loadFromJsonClampsOutOfRangeData();
    void loadFromJsonDuringDragMatchesThetis();
    void loadFromJsonRejectsTooFewPoints();
    void loadFromJsonRejectsBandCountMismatch();
    void loadFromJsonRejectsBadFreqRange();
    void loadFromJsonResetPathOnBandCountChange();
};

// -- Helpers --

// Hand-build a Thetis-shape JSON blob with three bands.  The strings used
// here pin the snake_case schema; if anyone "cleans up" key names in
// saveToJson without matching loadFromJson, this test catches it.
static QString buildThetisStyleJson(int bandCount,
                                    double globalGainDb,
                                    double freqMinHz,
                                    double freqMaxHz,
                                    bool   parametricEq,
                                    const QVector<double>& freqs,
                                    const QVector<double>& gains,
                                    const QVector<double>& qs) {
    QJsonObject root;
    root.insert(QStringLiteral("band_count"),       bandCount);
    root.insert(QStringLiteral("parametric_eq"),    parametricEq);
    root.insert(QStringLiteral("global_gain_db"),   globalGainDb);
    root.insert(QStringLiteral("frequency_min_hz"), freqMinHz);
    root.insert(QStringLiteral("frequency_max_hz"), freqMaxHz);

    QJsonArray pts;
    for (int i = 0; i < freqs.size(); ++i) {
        QJsonObject jp;
        jp.insert(QStringLiteral("frequency_hz"), freqs.at(i));
        jp.insert(QStringLiteral("gain_db"),      gains.at(i));
        jp.insert(QStringLiteral("q"),            qs.at(i));
        pts.append(jp);
    }
    root.insert(QStringLiteral("points"), pts);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

// -- Tests --

// Verify the JSON output uses Thetis Newtonsoft snake_case keys for every
// root field plus every point field.  Pins:
//   root: band_count, parametric_eq, global_gain_db, frequency_min_hz,
//         frequency_max_hz, points
//   point: frequency_hz, gain_db, q
void TestParametricEqJson::saveToJsonProducesSnakeCaseKeys() {
    ParametricEqJsonTester w;
    QString out = w.saveToJson();

    QJsonParseError perr;
    QJsonDocument   doc = QJsonDocument::fromJson(out.toUtf8(), &perr);
    QCOMPARE(perr.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject());

    QJsonObject root = doc.object();
    const QSet<QString> wantRoot = {
        QStringLiteral("band_count"),
        QStringLiteral("parametric_eq"),
        QStringLiteral("global_gain_db"),
        QStringLiteral("frequency_min_hz"),
        QStringLiteral("frequency_max_hz"),
        QStringLiteral("points"),
    };
    QSet<QString> gotRoot;
    for (const QString& k : root.keys()) gotRoot.insert(k);
    QCOMPARE(gotRoot, wantRoot);

    QJsonArray pts = root.value(QStringLiteral("points")).toArray();
    QVERIFY(pts.size() > 0);
    QJsonObject p0 = pts.at(0).toObject();
    const QSet<QString> wantPoint = {
        QStringLiteral("frequency_hz"),
        QStringLiteral("gain_db"),
        QStringLiteral("q"),
    };
    QSet<QString> gotPoint;
    for (const QString& k : p0.keys()) gotPoint.insert(k);
    QCOMPARE(gotPoint, wantPoint);
}

// Verify point objects DELIBERATELY omit band_id and band_color.  Thetis
// EqJsonPoint at ucParametricEq.cs:242-252 [v2.10.3.13] only carries
// frequency_hz/gain_db/q; adding bandId or bandColor would break round-
// trip with Thetis-generated JSON blobs that ride in MicProfileManager
// / TransmitModel.
void TestParametricEqJson::saveToJsonOmitsBandIdAndBandColor() {
    ParametricEqJsonTester w;
    QString out = w.saveToJson();

    QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
    QJsonObject   root = doc.object();
    QJsonArray    pts = root.value(QStringLiteral("points")).toArray();

    for (int i = 0; i < pts.size(); ++i) {
        QJsonObject p = pts.at(i).toObject();
        QVERIFY2(!p.contains(QStringLiteral("band_id")),
                 qPrintable(QStringLiteral("point %1 leaked band_id").arg(i)));
        QVERIFY2(!p.contains(QStringLiteral("band_color")),
                 qPrintable(QStringLiteral("point %1 leaked band_color").arg(i)));
        QVERIFY2(!p.contains(QStringLiteral("color")),
                 qPrintable(QStringLiteral("point %1 leaked color").arg(i)));
    }
}

// Verify Math.Round(_, 3) on frequency_hz: 1234.5678 should round to
// 1234.568 (half-to-even / banker's-rounding tie-break does not apply
// because the discarded digit is 8 > 5).
void TestParametricEqJson::saveToJsonRoundsFreqToThreeDecimals() {
    ParametricEqJsonTester w;
    // Index 4 is interior so it isn't pinned to the freq-min/max endpoint.
    w.pointsMut()[4].frequencyHz = 1234.5678;

    QString       out = w.saveToJson();
    QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
    QJsonArray    pts = doc.object().value(QStringLiteral("points")).toArray();

    double got = pts.at(4).toObject().value(QStringLiteral("frequency_hz")).toDouble();
    // Hand-computed: qRound(1234.5678 * 1000) = 1234568, /1000.0 = 1234.568.
    QCOMPARE(got, 1234.568);
}

// Verify Math.Round(_, 1) on gain_db: 5.49 -> 5.5; -3.12 -> -3.1.
void TestParametricEqJson::saveToJsonRoundsGainToOneDecimal() {
    ParametricEqJsonTester w;
    w.pointsMut()[1].gainDb = 5.49;
    w.pointsMut()[2].gainDb = -3.12;

    QString       out = w.saveToJson();
    QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
    QJsonArray    pts = doc.object().value(QStringLiteral("points")).toArray();

    double g1 = pts.at(1).toObject().value(QStringLiteral("gain_db")).toDouble();
    double g2 = pts.at(2).toObject().value(QStringLiteral("gain_db")).toDouble();
    // Hand-computed:
    //   qRound(5.49 * 10)  = 55,  /10 = 5.5
    //   qRound(-3.12 * 10) = -31, /10 = -3.1
    QCOMPARE(g1, 5.5);
    QCOMPARE(g2, -3.1);
}

// Verify Math.Round(_, 2) on q: 4.123 -> 4.12.
void TestParametricEqJson::saveToJsonRoundsQToTwoDecimals() {
    ParametricEqJsonTester w;
    w.pointsMut()[3].q = 4.123;

    QString       out = w.saveToJson();
    QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
    QJsonArray    pts = doc.object().value(QStringLiteral("points")).toArray();

    double got = pts.at(3).toObject().value(QStringLiteral("q")).toDouble();
    // Hand-computed: qRound(4.123 * 100) = 412, /100 = 4.12.
    QCOMPARE(got, 4.12);
}

// Save -> load round-trip on a non-default state: loaded freq/gain/q
// match the saved values within rounding tolerance.  Pins the entire
// schema chain (saveToJson keys + loadFromJson reader symmetry).
void TestParametricEqJson::loadFromJsonRoundTripPreservesData() {
    ParametricEqJsonTester saver;
    saver.seedBandsLinear();
    saver.setGlobalGainDirect(4.7);
    saver.setParametricEq(true);

    QString json = saver.saveToJson();

    ParametricEqJsonTester loader;
    bool ok = loader.loadFromJson(json);
    QVERIFY(ok);

    QCOMPARE(loader.pointCount(), saver.pointCount());
    QCOMPARE(loader.parametricEq(), saver.parametricEq());
    // global_gain_db round-trips through Math.Round(_, 1) on save AND on
    // load: 4.7 -> 4.7 (no precision loss).
    QCOMPARE(loader.globalGainDb(), 4.7);
    QCOMPARE(loader.frequencyMinHz(), saver.frequencyMinHz());
    QCOMPARE(loader.frequencyMaxHz(), saver.frequencyMaxHz());

    for (int i = 0; i < saver.pointCount(); ++i) {
        // Compare each axis independently; freq=3, gain=1, q=2 decimals.
        // The ROUND-trip must agree (no drift across save+load).
        const auto& sP = saver.pointsConst().at(i);
        const auto& lP = loader.pointsConst().at(i);
        // Endpoints (i=0 and i=N-1) are pinned to freq min/max by
        // enforceOrdering on both save AND load, so they round-trip
        // exactly.  Interior bands also round-trip cleanly because
        // the default 0..4000 even spread is in-order; the 5 Hz forced-
        // spacing sweep doesn't have to shift anything.
        double savedFreq = qRound(sP.frequencyHz * 1000.0) / 1000.0;
        QCOMPARE(lP.frequencyHz, savedFreq);

        double savedGain = qRound(sP.gainDb * 10.0) / 10.0;
        double savedQ    = qRound(sP.q * 100.0) / 100.0;
        QCOMPARE(qRound(lP.gainDb * 10.0) / 10.0, savedGain);
        QCOMPARE(qRound(lP.q * 100.0) / 100.0,    savedQ);
    }
}

// Verify loadFromJson does NOT touch existing m_points[i].bandId when the
// loaded band_count matches the existing point count.  Thetis source at
// cs:1518-1523 only calls resetPointsDefault when band_count differs;
// otherwise existing bandIds (from the ctor's resetPointsDefault) are
// preserved across the load.  This protects observers that pinned a
// per-bandId association (e.g. a colour swatch row).
void TestParametricEqJson::loadFromJsonRetainsExistingBandIds() {
    ParametricEqJsonTester w;

    // Snapshot existing bandIds before the load.
    QVector<int> originalBandIds;
    for (const auto& p : w.pointsConst()) originalBandIds.append(p.bandId);
    QCOMPARE(originalBandIds.size(), 10);

    // Build a JSON blob with the SAME band_count (=10) so the reset path
    // does NOT trigger.  freq/gain/q values irrelevant to this test.
    QVector<double> freqs(10);
    QVector<double> gains(10, 0.0);
    QVector<double> qs   (10, 4.0);
    for (int i = 0; i < 10; ++i) freqs[i] = 0.0 + i * (4000.0 / 9.0);

    QString json = buildThetisStyleJson(/*bandCount=*/10,
                                        /*globalGainDb=*/0.0,
                                        /*freqMinHz=*/0.0,
                                        /*freqMaxHz=*/4000.0,
                                        /*parametricEq=*/true,
                                        freqs, gains, qs);

    QVERIFY(w.loadFromJson(json));

    // bandIds must be preserved verbatim.
    for (int i = 0; i < w.pointCount(); ++i) {
        QCOMPARE(w.pointsConst().at(i).bandId, originalBandIds.at(i));
    }
}

// Garbage JSON -> false, no crash.
void TestParametricEqJson::loadFromJsonRejectsInvalidJson() {
    ParametricEqJsonTester w;

    QVector<double> freqsBefore;
    QVector<double> gainsBefore;
    QVector<double> qsBefore;
    w.getPointsData(freqsBefore, gainsBefore, qsBefore);

    QVERIFY(!w.loadFromJson(QStringLiteral("not even close to JSON {{{")));
    QVERIFY(!w.loadFromJson(QString())); // empty / whitespace
    QVERIFY(!w.loadFromJson(QStringLiteral("   \t\n  ")));

    // State must be unchanged after a rejected load.
    QVector<double> freqsAfter;
    QVector<double> gainsAfter;
    QVector<double> qsAfter;
    w.getPointsData(freqsAfter, gainsAfter, qsAfter);
    QCOMPARE(freqsAfter, freqsBefore);
    QCOMPARE(gainsAfter, gainsBefore);
    QCOMPARE(qsAfter,    qsBefore);
}

// JSON object missing the "points" array -> false.
void TestParametricEqJson::loadFromJsonRejectsMissingPoints() {
    ParametricEqJsonTester w;
    QVERIFY(!w.loadFromJson(QStringLiteral(R"({"foo": "bar"})")));
    QVERIFY(!w.loadFromJson(QStringLiteral(R"({"band_count": 10})")));
    // "points" present but not an array.
    QVERIFY(!w.loadFromJson(QStringLiteral(R"({"points": "not an array"})")));
}

// loadFromJson clamps freq into [freq_min_hz..freq_max_hz], gain into
// [m_dbMin..m_dbMax], q into [m_qMin..m_qMax].  Hand-computed expecteds
// trace each clamp branch.
void TestParametricEqJson::loadFromJsonClampsOutOfRangeData() {
    ParametricEqJsonTester w;
    // ctor defaults: dbMin=-24, dbMax=+24, qMin=0.2, qMax=30.
    // freq envelope from JSON: 0..4000 Hz.

    // 10 points: feed crazy values that all should clamp.
    QVector<double> freqs;
    QVector<double> gains;
    QVector<double> qs;
    for (int i = 0; i < 10; ++i) {
        freqs.append(99999.0);   // -> clamped to 4000.0
        gains.append(1000.0);    // -> clamped to +24.0
        qs.append(99999.0);      // -> clamped to qMax=30.0
    }

    QString json = buildThetisStyleJson(10, 999.0, 0.0, 4000.0, true, freqs, gains, qs);
    QVERIFY(w.loadFromJson(json));

    // global_gain_db also clamped to [dbMin..dbMax] = [-24, 24].
    QCOMPARE(w.globalGainDb(), 24.0);

    // Hand-computed: every freq was 99999 -> per-point clamp lands at 4000.
    // Then enforceOrdering(true) runs the 5 Hz forced-spacing sweep:
    //   * front endpoint pinned to freqMinHz=0
    //   * back endpoint pinned to freqMaxHz=4000
    //   * interior i in 1..8 clamped into [i*5, 4000 - (9-i)*5]
    //   * forward sweep: m_points[i] >= m_points[i-1] + 5
    //   * backward sweep: m_points[i] <= m_points[i+1] - 5
    // Net result for an all-equal-clamped input: each interior i lands
    // at its upper bound 4000 - (9-i)*5.
    static constexpr double expectedFreq[10] = {
        0.0,    // pinned front
        3960.0, // 4000 - 5*8
        3965.0, // 4000 - 5*7
        3970.0, // 4000 - 5*6
        3975.0, // 4000 - 5*5
        3980.0, // 4000 - 5*4
        3985.0, // 4000 - 5*3
        3990.0, // 4000 - 5*2
        3995.0, // 4000 - 5*1
        4000.0  // pinned back
    };

    for (int i = 0; i < w.pointCount(); ++i) {
        const auto& p = w.pointsConst().at(i);
        QCOMPARE(p.frequencyHz, expectedFreq[i]);
        QCOMPARE(p.gainDb, 24.0);
        QCOMPARE(p.q, 30.0);
    }
}

// Verify loadFromJson does NOT bail when a drag is in progress -- this
// matches Thetis source at cs:1488-1573 which has no drag-state guard.
// The plan body suggested adding a bail; source-first overrides that.
// Caller code wanting drag-safety must gate at the call site.
void TestParametricEqJson::loadFromJsonDuringDragMatchesThetis() {
    ParametricEqJsonTester w;
    w.forceDraggingPointActive();
    QVERIFY(w.draggingPoint());

    QVector<double> freqs(10);
    QVector<double> gains(10, 0.0);
    QVector<double> qs   (10, 4.0);
    for (int i = 0; i < 10; ++i) freqs[i] = 0.0 + i * (4000.0 / 9.0);

    QString json = buildThetisStyleJson(10, 0.0, 0.0, 4000.0, true, freqs, gains, qs);
    bool ok = w.loadFromJson(json);

    // Thetis-faithful: load proceeds even mid-drag.
    QVERIFY(ok);
}

// pts.size() < 2 -> false.
void TestParametricEqJson::loadFromJsonRejectsTooFewPoints() {
    ParametricEqJsonTester w;

    // Empty points array.
    QString empty = QStringLiteral(R"({"band_count": 0, "points": []})");
    QVERIFY(!w.loadFromJson(empty));

    // Single point.
    QJsonObject root;
    root.insert(QStringLiteral("band_count"),       1);
    root.insert(QStringLiteral("frequency_min_hz"), 0.0);
    root.insert(QStringLiteral("frequency_max_hz"), 1000.0);
    QJsonArray pts;
    QJsonObject jp;
    jp.insert(QStringLiteral("frequency_hz"), 100.0);
    jp.insert(QStringLiteral("gain_db"), 0.0);
    jp.insert(QStringLiteral("q"), 4.0);
    pts.append(jp);
    root.insert(QStringLiteral("points"), pts);

    QString json = QString::fromUtf8(QJsonDocument(root).toJson());
    QVERIFY(!w.loadFromJson(json));
}

// band_count != points.size() -> false (ucParametricEq.cs:1511).
void TestParametricEqJson::loadFromJsonRejectsBandCountMismatch() {
    ParametricEqJsonTester w;

    QVector<double> freqs(3);  freqs[0]=0; freqs[1]=2000; freqs[2]=4000;
    QVector<double> gains(3, 0.0);
    QVector<double> qs   (3, 4.0);

    // Lie: claim band_count=4 but provide 3 points.
    QString json = buildThetisStyleJson(/*bandCount=*/4,
                                        0.0, 0.0, 4000.0, true,
                                        freqs, gains, qs);
    QVERIFY(!w.loadFromJson(json));
}

// frequency_max_hz <= frequency_min_hz -> false (ucParametricEq.cs:1514).
void TestParametricEqJson::loadFromJsonRejectsBadFreqRange() {
    ParametricEqJsonTester w;

    QVector<double> freqs(3);  freqs[0]=0; freqs[1]=2000; freqs[2]=4000;
    QVector<double> gains(3, 0.0);
    QVector<double> qs   (3, 4.0);

    // freq_min > freq_max.
    QString invertedJson = buildThetisStyleJson(3, 0.0,
                                                /*freq_min=*/4000.0,
                                                /*freq_max=*/2000.0,
                                                true, freqs, gains, qs);
    QVERIFY(!w.loadFromJson(invertedJson));

    // freq_min == freq_max (equality also fails).
    QString equalJson = buildThetisStyleJson(3, 0.0, 1000.0, 1000.0, true,
                                             freqs, gains, qs);
    QVERIFY(!w.loadFromJson(equalJson));
}

// loadFromJson takes the resetPointsDefault path when band_count differs.
// Verifies the new band lineup matches the loaded count and the bandIds
// come back as 1..N from resetPointsDefault.
void TestParametricEqJson::loadFromJsonResetPathOnBandCountChange() {
    ParametricEqJsonTester w;
    QCOMPARE(w.pointCount(), 10);

    // Load a 5-band layout.
    QVector<double> freqs(5);
    QVector<double> gains(5, 0.0);
    QVector<double> qs   (5, 4.0);
    for (int i = 0; i < 5; ++i) freqs[i] = 0.0 + i * (4000.0 / 4.0);

    QString json = buildThetisStyleJson(5, 0.0, 0.0, 4000.0, true,
                                        freqs, gains, qs);
    QVERIFY(w.loadFromJson(json));
    QCOMPARE(w.pointCount(), 5);

    // resetPointsDefault assigns bandIds 1..N (ucParametricEq.cs:3163-3197).
    // Verify the new lineup is sequentially numbered.
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(w.pointsConst().at(i).bandId, i + 1);
    }
}

QTEST_MAIN(TestParametricEqJson)
#include "tst_parametric_eq_widget_json.moc"
