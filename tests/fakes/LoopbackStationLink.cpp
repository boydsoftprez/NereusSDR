// tests/fakes/LoopbackStationLink.cpp
//
// no-port-check: NereusSDR-original test fixture.

#include "LoopbackStationLink.h"

namespace NereusSDR::Test {

LoopbackStationLink::LoopbackStationLink(QObject* parent)
    : QObject(parent)
{
}

void LoopbackStationLink::sendFromClient(const QByteArray& wire)
{
    emit receivedByDaemon(wire);
}

void LoopbackStationLink::sendFromDaemon(const QByteArray& wire)
{
    emit receivedByClient(wire);
}

} // namespace NereusSDR::Test
