// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/DeviceWords.cpp  (NereusSDR)
// =================================================================
//
// See DeviceWords.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-07,
//               R-IOS-30), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "gui/multidevice/DeviceWords.h"

#include "core/WdspTypes.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/TransmitStateFacade.h"
#include "models/Band.h"
#include "models/SliceModel.h"

namespace NereusSDR::DeviceWords {

QString frequency(double hz)
{
    if (hz <= 0.0) {
        return {};
    }
    return QStringLiteral("%1 MHz").arg(hz / 1.0e6, 0, 'f', 3);
}

QString mode(int dspMode)
{
    if (dspMode < 0) {
        return {};
    }
    return SliceModel::modeName(static_cast<DSPMode>(dspMode));
}

QString band(int value)
{
    if (value < 0 || value >= static_cast<int>(Band::Count)) {
        return {};
    }
    return bandLabel(static_cast<Band>(value));
}

QString receiver(int streamIndex)
{
    return streamIndex >= 0 ? QStringLiteral("Receiver %1").arg(streamIndex + 1) : QString();
}

QString duration(qint64 seconds)
{
    if (seconds < 60) {
        return QStringLiteral("under a minute");
    }
    const qint64 minutes = seconds / 60;
    if (minutes < 60) {
        return minutes == 1 ? QStringLiteral("1 minute")
                            : QStringLiteral("%1 minutes").arg(minutes);
    }
    const qint64 hours = minutes / 60;
    const qint64 rest = minutes % 60;
    const QString h = hours == 1 ? QStringLiteral("1 hour") : QStringLiteral("%1 hours").arg(hours);
    if (rest == 0) {
        return h;
    }
    return rest == 1 ? h + QStringLiteral(" 1 minute")
                     : h + QStringLiteral(" %1 minutes").arg(rest);
}

QString sliceShort(const RemoteDeviceSlice& slice)
{
    const QString b = band(slice.band);
    return b.isEmpty() ? slice.letter : QStringLiteral("%1 %2").arg(slice.letter, b);
}

QString sliceLine(const QJsonObject& slice)
{
    QString letter = slice.value(QStringLiteral("letter")).toString();
    if (letter.isEmpty()) {
        const int id = slice.value(QStringLiteral("sliceId")).toInt(-1);
        letter = id >= 0 ? QString(QChar(u'A' + id)) : QStringLiteral("?");
    }
    QString text = QStringLiteral("Slice %1").arg(letter);
    const QString f = frequency(slice.value(QStringLiteral("frequencyHz")).toDouble());
    const QString m = mode(slice.value(QStringLiteral("mode")).toInt(-1));
    if (!f.isEmpty()) {
        text += QStringLiteral(", ") + f;
        if (!m.isEmpty()) {
            text += QLatin1Char(' ') + m;
        }
    }
    return text;
}

HolderBadge holderBadge(const TransmitState& tx, const QString& selfId)
{
    HolderBadge badge;
    if (tx.holderTransferring()) {
        badge.shown = true;
        badge.label = QStringLiteral("changing hands");
        badge.tone = HolderBadge::Tone::ChangingHands;
        badge.toolTip = QStringLiteral("Transmit is changing hands.");
        return badge;
    }
    if (tx.holderDeviceId().isEmpty() || (!selfId.isEmpty() && tx.holderDeviceId() == selfId)) {
        return badge;
    }
    badge.shown = true;
    const bool radio = tx.holderSource() == QStringLiteral("radioPtt");
    const QString shortName = radio ? QStringLiteral("Radio")
        : (!tx.holderShortName().isEmpty() ? tx.holderShortName()
           : (!tx.holderName().isEmpty() ? tx.holderName() : QStringLiteral("Another device")));
    const QString name = radio ? QStringLiteral("The radio")
        : (!tx.holderName().isEmpty() ? tx.holderName() : shortName);
    badge.label = shortName;
    if (tx.keyed()) {
        badge.tone = HolderBadge::Tone::OnAir;
        badge.toolTip = QStringLiteral("%1 has the transmitter and is on the air.").arg(name);
    } else if (tx.holderAway()) {
        badge.tone = HolderBadge::Tone::Away;
        badge.label = QStringLiteral("%1, away").arg(shortName);
        badge.toolTip = QStringLiteral("%1 has the transmitter and is away.").arg(name);
    } else {
        badge.tone = HolderBadge::Tone::Listening;
        badge.toolTip = QStringLiteral("%1 has the transmitter.").arg(name);
    }
    return badge;
}

} // namespace NereusSDR::DeviceWords
