// tst_band_link_fit.cpp
//
// no-port-check: NereusSDR-original test.
//
// 2 m on the station link (R-IOS-26, R-R3-49; link document section 6.1).
// A peer that did not declare band2m sees exactly the wire it was built
// for, in which 2 m is part of GEN; a Core without band2mVersion reads a
// window's per-band lists and maps without their 2 m entry.

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/session/BandLinkFit.h"
#include "models/Band.h"

using namespace NereusSDR;

namespace {

QJsonObject parse(const QByteArray& wire)
{
    return QJsonDocument::fromJson(wire).object();
}

QByteArray compact(const QJsonObject& o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QJsonObject entry(const char* name, const QJsonValue& value, const char* kind)
{
    return QJsonObject{{"ordinal", 0}, {"name", name}, {"kind", kind}, {"value", value}};
}

} // namespace

class TestBandLinkFit : public QObject {
    Q_OBJECT

private slots:
    void numbers_are_the_contract()
    {
        QCOMPARE(BandLinkFit::k2mNumber, static_cast<int>(Band::Band2m));
        QCOMPARE(BandLinkFit::kGenNumber, static_cast<int>(Band::GEN));
        QCOMPARE(QByteArray(BandLinkFit::kFeature), QByteArray("band2m"));
    }

    void a_slice_on_2m_reads_gen()
    {
        const QJsonObject delta{
            {"type", "delta"}, {"key", "slice:0"},
            {"properties", QJsonArray{entry("band", 27, "enum"),
                                      entry("frequency", 144200000.0, "f64")}}};
        const QJsonObject out = parse(BandLinkFit::forPeerWithout2m(compact(delta)));
        const QJsonArray props = out.value("properties").toArray();
        QCOMPARE(props.at(0).toObject().value("value").toInt(), 11);
        QCOMPARE(props.at(1).toObject().value("value").toDouble(), 144200000.0);
    }

    void a_message_without_2m_is_unchanged()
    {
        const QByteArray wire = compact(QJsonObject{
            {"type", "delta"}, {"key", "slice:0"},
            {"properties", QJsonArray{entry("band", 5, "enum"),
                                      entry("frequency", 14027000.0, "f64")}}});
        QCOMPARE(BandLinkFit::forPeerWithout2m(wire), wire);
        QCOMPARE(BandLinkFit::forStationWithout2m(wire), wire);
    }

    void a_spot_record_on_2m_reads_gen()
    {
        const QJsonObject batch{
            {"type", "record.batch"}, {"stream", "spots"}, {"generation", 1},
            {"reset", false}, {"removes", QJsonArray{}},
            {"upserts", QJsonArray{
                QJsonObject{{"id", "1"}, {"fields", QJsonObject{{"band", 27}, {"call", "W1AW"}}}},
                QJsonObject{{"id", "2"}, {"fields", QJsonObject{{"band", 10}, {"call", "K1AB"}}}}}}};
        const QJsonArray upserts =
            parse(BandLinkFit::forPeerWithout2m(compact(batch))).value("upserts").toArray();
        QCOMPARE(upserts.size(), 2);
        QCOMPARE(upserts.at(0).toObject().value("fields").toObject().value("band").toInt(), 11);
        QCOMPARE(upserts.at(1).toObject().value("fields").toObject().value("band").toInt(), 10);
    }

    void the_catalogue_drops_the_2m_button()
    {
        const QJsonObject catalogue{
            {"bands", QJsonArray{QJsonObject{{"id", 10}, {"label", "6"}},
                                 QJsonObject{{"id", 27}, {"label", "2"}},
                                 QJsonObject{{"id", 12}, {"label", "WWV"}}}}};
        const QJsonObject delta{
            {"type", "delta"}, {"key", "catalog"},
            {"properties", QJsonArray{entry("json", QString::fromUtf8(compact(catalogue)), "utf8")}}};
        const QJsonObject out = parse(BandLinkFit::forPeerWithout2m(compact(delta)));
        const QString text =
            out.value("properties").toArray().at(0).toObject().value("value").toString();
        const QJsonArray bands = QJsonDocument::fromJson(text.toUtf8()).object().value("bands").toArray();
        QCOMPARE(bands.size(), 2);
        QCOMPARE(bands.at(0).toObject().value("id").toInt(), 10);
        QCOMPARE(bands.at(1).toObject().value("id").toInt(), 12);
    }

    void a_labelled_2m_row_is_left_out_and_a_slice_reads_gen()
    {
        const QJsonObject message{
            {"type", "notice"},
            {"rows", QJsonArray{QJsonObject{{"band", 13}, {"label", "XVTR"}},
                                QJsonObject{{"band", 27}, {"label", "2m"}}}},
            {"slices", QJsonArray{QJsonObject{{"sliceId", 0}, {"letter", "A"}, {"band", 27}}}}};
        const QJsonObject out = parse(BandLinkFit::forPeerWithout2m(compact(message)));
        QCOMPARE(out.value("rows").toArray().size(), 1);
        QCOMPARE(out.value("slices").toArray().at(0).toObject().value("band").toInt(), 11);
    }

    void per_band_lists_and_maps_lose_2m_both_ways()
    {
        const QString list15 = QStringLiteral("1,1,1,1,1,1,1,1,1,1,1,1,1,1,3");
        QJsonObject watts;
        for (const Band b : kPerBandStateBands) {
            watts.insert(bandKeyName(b), 50);
        }
        const QJsonObject delta{
            {"type", "delta"}, {"key", "alexAntennas"},
            {"properties", QJsonArray{
                entry("rxAntennas", list15, "utf8"),
                entry("powerByBandJson",
                      QString::fromUtf8(QJsonDocument(watts).toJson(QJsonDocument::Compact)),
                      "utf8")}}};
        for (const QByteArray& fitted : {BandLinkFit::forPeerWithout2m(compact(delta)),
                                         BandLinkFit::forStationWithout2m(compact(delta))}) {
            const QJsonArray props = parse(fitted).value("properties").toArray();
            QCOMPARE(props.at(0).toObject().value("value").toString(),
                     QStringLiteral("1,1,1,1,1,1,1,1,1,1,1,1,1,1"));
            const QJsonObject map = QJsonDocument::fromJson(
                props.at(1).toObject().value("value").toString().toUtf8()).object();
            QCOMPARE(map.size(), 14);
            QVERIFY(!map.contains("2m"));
            QVERIFY(map.contains("GEN"));
        }
    }

    void a_window_to_an_older_core_keeps_band_numbers()
    {
        // A window never sends band 27 to a Core without 2 m (it refuses
        // first); the station direction touches only lists and maps.
        const QByteArray wire = compact(QJsonObject{
            {"type", "command.invoke"}, {"verb", "slice.selectBand"}, {"id", 3},
            {"args", QJsonArray{entry("sliceId", 0, "i64"), entry("band", 27, "i64")}}});
        QCOMPARE(BandLinkFit::forStationWithout2m(wire), wire);
    }
};

QTEST_APPLESS_MAIN(TestBandLinkFit)
#include "tst_band_link_fit.moc"
