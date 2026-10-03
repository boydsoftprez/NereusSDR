// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_disturbance_check.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 75 (R-IOS-30; the several-devices design, sections
// 7.1 and 7.2, rulings 7.3, 7.4 and 7.8): which other devices a change to a
// radio-wide setting disturbs, table-driven.
//
// Every row lays out a Core (whose slices sit on which receiver, fed from
// which ADC, at which frequency, and who holds transmit), names what a
// change touches, and states what each other device is told: its slices
// and each one's effect. The requester's own slices never count; a slice
// nobody owns has nobody to ask.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 75 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: transmit group fix wave 2 (R-IOS-02): the transmit slice
//               counts whoever owns it, ruling 8.11's frozen slice, and a
//               station holder nobody can ask. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/DisturbanceCheck.h"

using namespace NereusSDR;
using Effect = DisturbanceCheck::Effect;

namespace {

const QByteArray kA("device-a");
const QByteArray kB("device-b");
const QByteArray kC("device-c");

DisturbanceCheck::SliceInfo slice(int id, const QByteArray& device, int stream, int adc,
                                  double frequencyHz, int low = 100, int high = 3000)
{
    return DisturbanceCheck::SliceInfo{id, device, stream, adc, frequencyHz, low, high};
}

// "b:1=changes,2=moves;c:3=closes" (a holder with no slices: "c+:").
QString describe(const QList<DisturbanceCheck::Affected>& affected)
{
    QStringList devices;
    for (const DisturbanceCheck::Affected& a : affected) {
        QStringList slices;
        for (const DisturbanceCheck::AffectedSlice& s : a.slices) {
            slices.append(QStringLiteral("%1=%2").arg(s.sliceId).arg(
                DisturbanceCheck::effectName(s.effect)));
        }
        QString name = QString::fromLatin1(a.device).remove(QStringLiteral("device-"));
        if (a.holdsTransmit) {
            name += QLatin1Char('+');
        }
        devices.append(name + QLatin1Char(':') + slices.join(QLatin1Char(',')));
    }
    return devices.join(QLatin1Char(';'));
}

struct Row {
    const char* name;
    DisturbanceCheck::Topology topology;
    DisturbanceCheck::Scope scope;
    QByteArray requester;
    QString expected;
    bool onAirRefused = false;
};

// A 2-ADC board (a G2): A on receiver 0 (ADC0), B on receiver 1 (ADC0) and
// receiver 2 (ADC1, on EXT1), C on receiver 3 (ADC1); one slice nobody owns
// on receiver 4 (ADC0).
DisturbanceCheck::Topology twoAdcCore()
{
    DisturbanceCheck::Topology t;
    t.slices = {
        slice(0, kA, 0, 0, 7074000.0),
        slice(1, kB, 1, 0, 14074000.0),
        slice(2, kB, 2, 1, 21074000.0),
        slice(3, kC, 3, 1, 28074000.0),
        slice(4, QByteArray(), 4, 0, 3573000.0),
    };
    return t;
}

// A 1-ADC board (an HL2): A and B share receiver 0; C alone on receiver 1.
DisturbanceCheck::Topology oneAdcCore()
{
    DisturbanceCheck::Topology t;
    t.slices = {
        slice(0, kA, 0, 0, 7074000.0),
        slice(1, kB, 0, 0, 7150000.0),
        slice(2, kC, 1, 0, 14074000.0),
    };
    return t;
}

QList<Row> rows()
{
    QList<Row> r;
    {
        // Protocol 1: the rate is the radio's; every receiver changes. The
        // plan (RadioModel::planSampleRateReach) keeps B's slice on its
        // receiver and closes C's, which no longer fits anywhere.
        Row row{"protocol 1 rate change", oneAdcCore(), {}, kA,
                QStringLiteral("b:1=changes;c:2=closes")};
        row.scope.radio = true;
        row.scope.stopsDataFlow = true;
        row.scope.planned = {{0, Effect::Changes}, {1, Effect::Changes}, {2, Effect::Closes}};
        r.append(row);
    }
    {
        // Protocol 2 on a shared receiver: only that receiver's rate; B's
        // slice there moves to a free receiver, C's elsewhere is untouched.
        Row row{"protocol 2 rate change on a shared receiver", oneAdcCore(), {}, kA,
                QStringLiteral("b:1=moves")};
        row.scope.receivers = {0};
        row.scope.planned = {{1, Effect::Moves}};
        r.append(row);
    }
    {
        // Protocol 2 on a shared receiver with no receiver free.
        Row row{"protocol 2 rate change, nowhere to go", oneAdcCore(), {}, kA,
                QStringLiteral("b:1=closes")};
        row.scope.receivers = {0};
        row.scope.planned = {{1, Effect::Closes}};
        r.append(row);
    }
    {
        // The ADC0 preamp on a 2-ADC board: B's ADC0 slice changes, its
        // EXT1 slice on ADC1 does not; C (ADC1 only) is not asked; the
        // slice nobody owns has nobody to ask.
        Row row{"ADC0 preamp, slices on both ADCs", twoAdcCore(), {}, kA,
                QStringLiteral("b:1=changes")};
        row.scope.adcs = {0};
        r.append(row);
    }
    {
        // The ADC1 preamp: B's EXT1 slice and C.
        Row row{"ADC1 preamp", twoAdcCore(), {}, kA, QStringLiteral("b:2=changes;c:3=changes")};
        row.scope.adcs = {1};
        r.append(row);
    }
    {
        // PureSignal on a 1-ADC board: every user receiver stops while the
        // holder transmits; the holder (C, not keyed) is also disturbed.
        DisturbanceCheck::Topology t = oneAdcCore();
        t.transmit.holder = kC;
        t.transmit.txSliceId = 2;
        Row row{"PureSignal on a 1-ADC board", t, {}, kA,
                QStringLiteral("b:1=pausesWhileTransmitting;c+:2=pausesWhileTransmitting")};
        row.scope.adcs = {0};
        row.scope.effect = Effect::PausesWhileTransmitting;
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    {
        // PureSignal on a 2-ADC board: receivers keep running; only the
        // holder is disturbed, with no slices of its own named.
        DisturbanceCheck::Topology t = twoAdcCore();
        t.transmit.holder = kB;
        t.transmit.txSliceId = 1;
        Row row{"PureSignal on a 2-ADC board", t, {}, kA, QStringLiteral("b+:")};
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    {
        // Nobody holds transmit: a transmitter-only change disturbs nobody.
        Row row{"the amplifier with nobody holding transmit", twoAdcCore(), {}, kA, QString()};
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    {
        // Diversity on a 2-ADC board: receiver 0.
        DisturbanceCheck::Topology t = twoAdcCore();
        t.slices[0].device = kB;  // B on receiver 0 now; A asks from elsewhere
        t.slices[1].device = kA;
        Row row{"diversity on a 2-ADC board", t, {}, kA, QStringLiteral("b:0=changes")};
        row.scope.receivers = {0};
        r.append(row);
    }
    {
        // Diversity on a 1-ADC board: every receiver.
        Row row{"diversity on a 1-ADC board", oneAdcCore(), {}, kA,
                QStringLiteral("b:1=changes;c:2=changes")};
        row.scope.radio = true;
        r.append(row);
    }
    {
        // A shared receiver's noise blanker: that receiver's slices.
        Row row{"a shared blanker", oneAdcCore(), {}, kA, QStringLiteral("b:1=changes")};
        row.scope.receivers = {0};
        r.append(row);
    }
    {
        // A notch at 14.075 MHz is inside C's passband (14.0741..14.0770
        // MHz), not B's.
        Row row{"a notch inside another device's passband", oneAdcCore(), {}, kA,
                QStringLiteral("c:2=changes")};
        row.scope.ranges = {{14075000.0 - 50.0, 14075000.0 + 50.0}};
        r.append(row);
    }
    {
        // Touching the passband's edge counts; a notch just past it does not.
        Row edge{"a notch on a passband edge", oneAdcCore(), {}, kA, QStringLiteral("c:2=changes")};
        edge.scope.ranges = {{14077000.0, 14077100.0}};
        r.append(edge);
        Row past{"a notch past a passband", oneAdcCore(), {}, kA, QString()};
        past.scope.ranges = {{14077001.0, 14077100.0}};
        r.append(past);
    }
    {
        // The tuner's antenna on a 2-ADC board: ADC0's receivers (ruling
        // 7.2) and the transmitter, whose holder C listens only on ADC1.
        DisturbanceCheck::Topology t = twoAdcCore();
        t.transmit.holder = kC;
        t.transmit.txSliceId = 3;
        Row row{"the tuner's antenna", t, {}, kA, QStringLiteral("b:1=changes;c+:")};
        row.scope.adcs = {0};
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    {
        // The radio choice: every slice.
        Row row{"the radio choice", twoAdcCore(), {}, kA,
                QStringLiteral("b:1=changes,2=changes;c:3=changes")};
        row.scope.radio = true;
        r.append(row);
    }
    {
        // The requester's own slices never count, even on the receiver the
        // change touches; with only its own there, nobody is asked.
        DisturbanceCheck::Topology t;
        t.slices = {slice(0, kA, 0, 0, 7074000.0), slice(1, kA, 0, 0, 7100000.0)};
        Row row{"only the requester's own slices", t, {}, kA, QString()};
        row.scope.radio = true;
        r.append(row);
    }
    // ── Ruling 7.4 (D60): refused while the holder is on the air ─────────
    {
        DisturbanceCheck::Topology t = oneAdcCore();
        t.transmit = {kC, true, 2};
        Row row{"on the air: a Protocol 1 rate change", t, {}, kA,
                QStringLiteral("b:1=changes;c+:2=changes"), true};
        row.scope.radio = true;
        row.scope.stopsDataFlow = true;
        r.append(row);
    }
    {
        DisturbanceCheck::Topology t = oneAdcCore();
        t.transmit = {kB, true, 1};
        Row row{"on the air: a rate change moving the transmit slice", t, {}, kA,
                QStringLiteral("b+:1=moves"), true};
        row.scope.receivers = {0};
        row.scope.planned = {{1, Effect::Moves}};
        r.append(row);
    }
    {
        // A slice of the holder's that is not its transmit slice may move.
        DisturbanceCheck::Topology t = oneAdcCore();
        t.slices.append(slice(3, kB, 1, 0, 14100000.0));
        t.transmit = {kB, true, 3};
        Row row{"on the air: another of the holder's slices moves", t, {}, kA,
                QStringLiteral("b+:1=moves"), false};
        row.scope.receivers = {0};
        row.scope.planned = {{1, Effect::Moves}};
        r.append(row);
    }
    for (const char* name : {"on the air: the amplifier", "on the air: the tuner",
                             "on the air: an antenna", "on the air: PureSignal",
                             "on the air: the interlock", "on the air: the power cap"}) {
        DisturbanceCheck::Topology t = twoAdcCore();
        t.transmit = {kC, true, 3};
        Row row{name, t, {}, kA, QStringLiteral("c+:"), true};
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    {
        // Not keyed: asked, not refused.
        DisturbanceCheck::Topology t = twoAdcCore();
        t.transmit = {kC, false, 3};
        Row row{"holding but not on the air: the amplifier", t, {}, kA, QStringLiteral("c+:"),
                false};
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    {
        // The holder's own change is not refused by this rule, and its own
        // slices do not count.
        DisturbanceCheck::Topology t = twoAdcCore();
        t.transmit = {kA, true, 0};
        Row row{"the holder's own change on the air", t, {}, kA, QString(), false};
        row.scope.transmitter = true;
        row.scope.transmitPath = true;
        r.append(row);
    }
    return r;
}

} // namespace

class TstDisturbanceCheck : public QObject {
    Q_OBJECT

private slots:
    void eachRowNamesWhatItDisturbs()
    {
        for (const Row& row : rows()) {
            const QList<DisturbanceCheck::Affected> affected =
                DisturbanceCheck::check(row.scope, row.topology, row.requester);
            QCOMPARE(describe(affected), row.expected);
            QVERIFY2(DisturbanceCheck::refusedOnAir(row.scope, row.topology, row.requester,
                                                    affected)
                         == row.onAirRefused,
                     row.name);
            for (const DisturbanceCheck::Affected& a : affected) {
                QVERIFY2(a.device != row.requester, row.name);
                QVERIFY2(!a.device.isEmpty(), row.name);
            }
        }
    }

    // Fix wave 2, Important 3 (rulings 7.4 and 8.11): while the station
    // device is keyed on another device's slice, the holder owns no slice,
    // yet a move, a close or a rate change reaching that slice waits,
    // whoever asks, the slice's owner included.
    void theFrozenSliceCountsWhoeverOwnsIt()
    {
        DisturbanceCheck::Topology t = twoAdcCore();
        t.transmit.holder = QByteArrayLiteral("station");
        t.transmit.keyed = true;
        t.transmit.txSliceId = 1;
        t.transmit.frozenSliceId = 1;
        t.transmit.holderAskable = false;

        // A's Protocol 2 rate change moves B's frozen slice.
        DisturbanceCheck::Scope moves;
        moves.receivers = {1};
        moves.planned.insert(1, Effect::Moves);
        QList<DisturbanceCheck::Affected> affected = DisturbanceCheck::check(moves, t, kA);
        QCOMPARE(describe(affected), QStringLiteral("b:1=moves"));
        QVERIFY(DisturbanceCheck::refusedOnAir(moves, t, kA, affected));

        // A pan move closing it, by a device that does not own it.
        DisturbanceCheck::Scope closes;
        closes.receivers = {1};
        closes.effect = Effect::Closes;
        affected = DisturbanceCheck::check(closes, t, kC);
        QVERIFY(DisturbanceCheck::refusedOnAir(closes, t, kC, affected));

        // B's own rate change of the frozen slice's receiver, which only
        // changes it: its own slice never counts in check(), and the
        // freeze still refuses it.
        DisturbanceCheck::Scope own;
        own.receivers = {1};
        own.planned.insert(1, Effect::Changes);
        affected = DisturbanceCheck::check(own, t, kB);
        QVERIFY(affected.isEmpty());
        QVERIFY(DisturbanceCheck::refusedOnAir(own, t, kB, affected));

        // A change that only changes it (a shared noise blanker) is not a
        // take, move or rate change.
        DisturbanceCheck::Scope blanker;
        blanker.receivers = {1};
        affected = DisturbanceCheck::check(blanker, t, kA);
        QVERIFY(!DisturbanceCheck::refusedOnAir(blanker, t, kA, affected));

        // A saved accessory address: the transmitter, not its path. The
        // station device has nobody to ask, so it applies at once.
        DisturbanceCheck::Scope address;
        address.transmitter = true;
        affected = DisturbanceCheck::check(address, t, kA);
        QVERIFY(affected.isEmpty());
        QVERIFY(!DisturbanceCheck::refusedOnAir(address, t, kA, affected));
        // The same with a device holding: it is asked, not refused.
        t.transmit = {kC, true, 3};
        affected = DisturbanceCheck::check(address, t, kA);
        QCOMPARE(describe(affected), QStringLiteral("c+:"));
        QVERIFY(!DisturbanceCheck::refusedOnAir(address, t, kA, affected));

        // Unkeyed, nothing is frozen.
        t.transmit = {QByteArrayLiteral("station"), false, 1};
        t.transmit.holderAskable = false;
        affected = DisturbanceCheck::check(own, t, kB);
        QVERIFY(!DisturbanceCheck::refusedOnAir(own, t, kB, affected));
    }

    void effectNamesAreTheWireWords()
    {
        QCOMPARE(DisturbanceCheck::effectName(Effect::Changes), QStringLiteral("changes"));
        QCOMPARE(DisturbanceCheck::effectName(Effect::Moves), QStringLiteral("moves"));
        QCOMPARE(DisturbanceCheck::effectName(Effect::Closes), QStringLiteral("closes"));
        QCOMPARE(DisturbanceCheck::effectName(Effect::PausesWhileTransmitting),
                 QStringLiteral("pausesWhileTransmitting"));
    }

    void aSliceWithNoReceiverIsReachedOnlyByTheRadioOrItsPassband()
    {
        const DisturbanceCheck::SliceInfo loose = slice(0, kB, -1, -1, 7074000.0);
        DisturbanceCheck::Scope adc;
        adc.adcs = {0, 1};
        adc.receivers = {0};
        QVERIFY(!DisturbanceCheck::reaches(adc, loose));
        DisturbanceCheck::Scope radio;
        radio.radio = true;
        QVERIFY(DisturbanceCheck::reaches(radio, loose));
    }
};

QTEST_GUILESS_MAIN(TstDisturbanceCheck)
#include "tst_disturbance_check.moc"
