// no-port-check: NereusSDR-original. See RadioInfoFacts.h.
#include "core/RadioInfoFacts.h"

#include "core/SampleRateCatalog.h"

namespace NereusSDR {

QString RadioInfoFacts::supportText() const
{
    return QStringLiteral(
        "Board: %1\n"
        "Protocol: %2\n"
        "ADC count: %3\n"
        "Max RX: %4\n"
        "Firmware: %5\n"
        "MAC: %6\n"
        "IP: %7\n"
        "Max sample rate: %8 Hz\n")
        .arg(board, protocol, adcCount, maxRx, firmware, mac, ip)
        .arg(maxSampleRateHz);
}

RadioInfoFacts radioInfoFacts(const RadioInfo& info, const BoardCapabilities& caps,
                              HPSDRModel model)
{
    // The tab's placeholder for a value not reported (U+2014).
    const QString none = QString(QChar(0x2014));
    RadioInfoFacts facts;
    facts.board = info.name.isEmpty() ? QString::fromLatin1(caps.displayName) : info.name;
    facts.protocol = info.protocol == ProtocolVersion::Protocol2
        ? QStringLiteral("Protocol 2") : QStringLiteral("Protocol 1");
    facts.adcCount = QString::number(caps.adcCount);
    // The radio's own receiver count where it reported one, the board row's
    // otherwise: the same count the stream pool is sized from.
    facts.maxRx = QString::number(BoardCapsTable::effectiveReceiverCount(
        caps, info.protocol, info.reportedReceivers));
    facts.firmware = info.firmwareVersion > 0 ? QString::number(info.firmwareVersion) : none;
    facts.mac = info.macAddress.isEmpty() ? none : info.macAddress;
    facts.ip = info.address.isNull() ? none : info.address.toString();
    // Plan Task 5: the top rate for the protocol the radio is running (the
    // last entry of the list the sample rate box is built from), not the
    // row's top, which spans both protocols on boards that run either.
    const auto allowed = allowedSampleRates(info.protocol, caps, model);
    facts.maxSampleRateHz = allowed.empty() ? 0 : allowed.back();
    return facts;
}

} // namespace NereusSDR
